#include "wmbus_meter.h"

namespace esphome {
namespace wmbus_meter {
static const char *TAG = "wmbus_meter";

void Meter::set_meter_params(std::string id, std::string driver,
                             std::string key,
                             std::initializer_list<LinkMode> linkModes) {
  MeterInfo meter_info;
  meter_info.parse(driver + '-' + id, driver, id + ",", key);

  this->meter = createMeter(&meter_info);

  if (this->meter == nullptr) {
    ESP_LOGE(TAG, "Failed to create meter driver '%s' for meter_id=0x%s", driver.c_str(), id.c_str());
    ESP_LOGE(TAG, "This usually means the driver was not compiled in. Ensure wmbus_common: drivers includes '%s' (or set drivers: all).", driver.c_str());
    this->mark_failed();
    return;
  }

  for (auto linkMode : linkModes)
    this->link_modes_.addLinkMode(linkMode);
}

void Meter::set_radio(wmbus_radio::Radio *radio) {
  this->radio = radio;
  if (this->radio == nullptr) {
    ESP_LOGE(TAG, "Radio not set");
    this->mark_failed();
    return;
  }
  radio->add_frame_handler(
      [this](wmbus_radio::Frame *frame) { return this->handle_frame(frame); });
}

void Meter::setup() {
  // Work queue: main loop posts frame data here; capacity 4 so that a short
  // burst of frames from different meters doesn't get dropped immediately.
  this->parse_work_queue_ = xQueueCreate(4, sizeof(ParseWork *));
  // Result queue: parser posts matched results here; capacity 1 ensures the
  // parser blocks (via result_consumed_sem_) until the main loop has finished
  // reading ::Meter state before the next handleTelegram() write begins.
  this->parse_result_queue_ = xQueueCreate(1, sizeof(ParseResult *));
  // Binary semaphore initialised to 0 (taken). The parser waits on this after
  // posting a result; the main loop gives it after consuming the result.
  this->result_consumed_sem_ = xSemaphoreCreateBinary();

  if (!this->parse_work_queue_ || !this->parse_result_queue_ ||
      !this->result_consumed_sem_) {
    ESP_LOGE(TAG, "Failed to create parser queues/semaphore");
    this->mark_failed();
    return;
  }

  if (xTaskCreate(Meter::parser_task, "wmbus_parse",
                  8 * 1024, this, 1,
                  &this->parser_task_handle_) != pdPASS) {
    ESP_LOGE(TAG, "Failed to create parser task");
    this->mark_failed();
  }
}

void Meter::loop() {
  if (this->parse_result_queue_ == nullptr)
    return;

  ParseResult *result;
  if (xQueueReceive(this->parse_result_queue_, &result, 0) != pdPASS)
    return;

  // The parser task is blocked on result_consumed_sem_, so ::Meter state is
  // stable (no concurrent writes) for the duration of this block.
  ESP_LOGI(TAG, "Telegram matched %s (RSSI: %d dBm, mode: %s)",
           this->meter->name().c_str(), result->rssi, toString(result->lm));
  this->last_telegram.reset(result->telegram);
  delete result;

  this->on_telegram_callback_manager();
  this->last_telegram = nullptr;

  // Allow the parser task to proceed with the next work item.
  xSemaphoreGive(this->result_consumed_sem_);
}

void Meter::dump_config() {
  if (this->meter == nullptr) {
    ESP_LOGCONFIG(TAG, "wM-Bus Meter:");
    ESP_LOGCONFIG(TAG, "  Driver: (not initialized)");
    ESP_LOGCONFIG(TAG, "  Status: FAILED (driver not available)");
    return;
  }

  std::string id = this->get_id();
  std::string driver = this->get_driver();
  std::string key = this->get_key();

  ESP_LOGCONFIG(TAG, "wM-Bus Meter:");
  ESP_LOGCONFIG(TAG, "  ID: 0x%s", id.c_str());
  ESP_LOGCONFIG(TAG, "  Driver: %s", driver.c_str());
  ESP_LOGCONFIG(TAG, "  Key: %s", key.c_str());
}

std::string Meter::get_id() {
  if (this->meter == nullptr)
    return "unknown";
  std::vector<AddressExpression> address_expressions =
      this->meter->addressExpressions();
  return address_expressions.size() > 0 ? address_expressions[0].id : "unknown";
}

std::string Meter::get_driver() {
  return this->meter != nullptr ? this->meter->driverName().str() : "unknown";
}

std::string Meter::get_key() {
  if (this->meter == nullptr)
    return "unknown";
  MeterKeys *keys = this->meter->meterKeys();
  return keys->hasConfidentialityKey() ? bin2hex(keys->confidentiality_key)
                                       : "not-encrypted";
}

void Meter::handle_frame(wmbus_radio::Frame *frame) {
  if (this->is_failed() || this->meter == nullptr ||
      this->parse_work_queue_ == nullptr)
    return;

  if (!this->link_modes_.has(frame->link_mode()))
    return;

  auto about =
      AboutTelegram(App.get_friendly_name(), frame->rssi(), FrameType::WMBUS);

  // Quick DLL-header-only address pre-check (~1 ms) to avoid posting work for
  // frames that clearly don't belong to this meter.
  Telegram header_t;
  header_t.about = about;
  if (!header_t.parseHeader(frame->data()))
    return;

  bool used_wildcard = false;
  auto &aes = this->meter->addressExpressions();
  if (!doesTelegramMatchExpressions(header_t.addresses, aes, &used_wildcard))
    return;

  // Mark the frame as handled so Radio::loop() logs "Telegram handled by N
  // handlers" at the end of this (fast) main-loop iteration.
  frame->mark_as_handled();

  // Post raw frame data to the background parser task. Use a 0 timeout so
  // this call never blocks the main loop; drop the frame if the work queue
  // is momentarily full (extremely rare with capacity-4 queue).
  auto *work = new ParseWork{frame->data(), App.get_friendly_name(),
                             frame->rssi(), frame->link_mode()};
  if (xQueueSend(this->parse_work_queue_, &work, 0) != pdTRUE) {
    ESP_LOGW(TAG, "Parser work queue full, dropping frame for %s",
             this->meter->name().c_str());
    delete work;
  }
}

// Background FreeRTOS task: performs the expensive handleTelegram() (AES
// decryption + DV field extraction) completely off the ESPHome main loop.
void Meter::parser_task(void *pvParameters) {
  Meter *self = static_cast<Meter *>(pvParameters);
  while (true) {
    ParseWork *work;
    xQueueReceive(self->parse_work_queue_, &work, portMAX_DELAY);

    int8_t rssi = work->rssi;
    LinkMode lm = work->lm;
    auto about =
        AboutTelegram(work->friendly_name, rssi, FrameType::WMBUS);
    // Move the frame bytes to avoid an extra copy; work is freed after parse.
    std::vector<uchar> frame_data = std::move(work->frame_data);
    delete work;

    std::vector<Address> addresses;
    bool id_match = false;
    auto *telegram = new Telegram();

    self->meter->handleTelegram(about, frame_data, false, &addresses,
                                &id_match, telegram);

    if (!id_match) {
      delete telegram;
      continue;
    }

    auto *result = new ParseResult{telegram, rssi, lm};
    // Post to the result queue. portMAX_DELAY ensures the parser blocks here
    // if the main loop has not yet consumed the previous result — preventing
    // concurrent writes/reads of the ::Meter internal field storage.
    xQueueSend(self->parse_result_queue_, &result, portMAX_DELAY);

    // Wait until Meter::loop() has finished firing callbacks (reading ::Meter
    // state) before this task is allowed to call handleTelegram() again
    // (writing ::Meter state).
    xSemaphoreTake(self->result_consumed_sem_, portMAX_DELAY);
  }
}

std::string Meter::as_json(bool pretty_print) {
  if (this->meter == nullptr || this->last_telegram == nullptr)
    return "{}";
  std::string json;
  this->meter->printMeter(this->last_telegram.get(), nullptr, nullptr, '\t',
                          &json, nullptr, nullptr, nullptr, pretty_print);
  return json;
}

optional<std::string> Meter::get_string_field(std::string field_name) {

  if (this->meter == nullptr)
    return {};

  if (field_name == "timestamp")
    return this->meter->datetimeOfUpdateHumanReadable();

  if (field_name == "timestamp_zulu")
    return this->meter->datetimeOfUpdateRobot();

  auto field_info = this->meter->findFieldInfo(field_name, Quantity::Text);
  if (field_info)
    return this->meter->getStringValue(field_info);

  return {};
}

optional<float> Meter::get_numeric_field(std::string field_name) {
  // RSSI is not handled by meter but by telegram :/
  if (field_name == "rssi_dbm") {
    if (this->last_telegram == nullptr)
      return {};
    return this->last_telegram->about.rssi_dbm;
  }

  if (this->meter == nullptr)
    return {};

  if (field_name == "timestamp")
    return this->meter->timestampLastUpdate();

  std::string name;
  Unit unit;
  extractUnit(field_name, &name, &unit);

  auto value = this->meter->getNumericValue(name, unit);

  if (!std::isnan(value))
    return value;

  return {};
}

void Meter::on_telegram(std::function<void()> &&callback) {
  this->on_telegram_callback_manager.add(std::move(callback));
}

} // namespace wmbus_meter
} // namespace esphome
