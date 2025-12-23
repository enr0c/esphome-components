#include "wmbus_unhandled.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {
namespace wmbus_unhandled {

static const char *const TAG = "wmbus_unhandled";

void UnhandledMeterTracker::setup() {
  if (this->radio_ == nullptr) {
    ESP_LOGE(TAG, "Radio not set!");
    this->mark_failed();
    return;
  }
  
  // Register frame handler
  this->radio_->add_frame_handler([this](wmbus_radio::Frame *frame) {
    this->handle_frame_(frame);
  });
  
  ESP_LOGI(TAG, "Unhandled meter tracker initialized");
}

void UnhandledMeterTracker::loop() {
  // Nothing to do in loop, frame handler does the work
}

void UnhandledMeterTracker::dump_config() {
  ESP_LOGCONFIG(TAG, "Unhandled Meter Tracker:");
  ESP_LOGCONFIG(TAG, "  Tracking %d unhandled meters", this->unhandled_meters_.size());
}

void UnhandledMeterTracker::handle_frame_(wmbus_radio::Frame *frame) {
  // Only process frames that were not handled by any meter
  if (frame->handlers_count() > 0) {
    return;  // Frame was handled, not interested
  }
  
  // Parse the telegram to get the meter ID
  Telegram t;
  if (!t.parseHeader(frame->data()) || t.addresses.empty()) {
    return;  // Cannot parse telegram or no address found
  }
  
  std::string meter_id = t.addresses.back().id;
  int rssi = frame->rssi();
  uint32_t now = millis();
  
  // Check if this is a new unhandled meter
  if (this->unhandled_meters_.find(meter_id) == this->unhandled_meters_.end()) {
    ESP_LOGI(TAG, "New unhandled meter detected: %s (RSSI: %d dBm)", 
             meter_id.c_str(), rssi);
    this->create_sensors_for_meter_(meter_id);
  }
  
  // Update meter info
  UnhandledMeterInfo &info = this->unhandled_meters_[meter_id];
  info.id = meter_id;
  info.rssi = rssi;
  info.last_seen_millis = now;
  info.last_seen = this->format_timestamp_(now);
  
  // Update sensors
  if (this->id_sensors_[meter_id] != nullptr) {
    this->id_sensors_[meter_id]->publish_state(meter_id);
  }
  if (this->last_seen_sensors_[meter_id] != nullptr) {
    this->last_seen_sensors_[meter_id]->publish_state(info.last_seen);
  }
  if (this->rssi_sensors_[meter_id] != nullptr) {
    this->rssi_sensors_[meter_id]->publish_state(std::to_string(rssi) + " dBm");
  }
}

void UnhandledMeterTracker::create_sensors_for_meter_(const std::string &meter_id) {
  // Create text sensors for this meter
  auto *id_sensor = new text_sensor::TextSensor();
  id_sensor->set_name(("Unhandled Meter " + meter_id + " ID").c_str());
  id_sensor->set_object_id(("unhandled_meter_" + meter_id + "_id").c_str());
  id_sensor->set_entity_category(esphome::ENTITY_CATEGORY_DIAGNOSTIC);
  App.register_text_sensor(id_sensor);
  this->id_sensors_[meter_id] = id_sensor;
  
  auto *last_seen_sensor = new text_sensor::TextSensor();
  last_seen_sensor->set_name(("Unhandled Meter " + meter_id + " Last Seen").c_str());
  last_seen_sensor->set_object_id(("unhandled_meter_" + meter_id + "_last_seen").c_str());
  last_seen_sensor->set_entity_category(esphome::ENTITY_CATEGORY_DIAGNOSTIC);
  App.register_text_sensor(last_seen_sensor);
  this->last_seen_sensors_[meter_id] = last_seen_sensor;
  
  auto *rssi_sensor = new text_sensor::TextSensor();
  rssi_sensor->set_name(("Unhandled Meter " + meter_id + " RSSI").c_str());
  rssi_sensor->set_object_id(("unhandled_meter_" + meter_id + "_rssi").c_str());
  rssi_sensor->set_entity_category(esphome::ENTITY_CATEGORY_DIAGNOSTIC);
  App.register_text_sensor(rssi_sensor);
  this->rssi_sensors_[meter_id] = rssi_sensor;
}

std::string UnhandledMeterTracker::format_timestamp_(uint32_t millis_val) {
  // Format using uptime (days, hours, minutes, seconds)
  uint32_t seconds = millis_val / 1000;
  uint32_t minutes = seconds / 60;
  uint32_t hours = minutes / 60;
  uint32_t days = hours / 24;
  
  seconds = seconds % 60;
  minutes = minutes % 60;
  hours = hours % 24;
  
  char buffer[64];
  if (days > 0) {
    snprintf(buffer, sizeof(buffer), "%ud %02uh %02um %02us", days, hours, minutes, seconds);
  } else if (hours > 0) {
    snprintf(buffer, sizeof(buffer), "%uh %02um %02us", hours, minutes, seconds);
  } else if (minutes > 0) {
    snprintf(buffer, sizeof(buffer), "%um %02us", minutes, seconds);
  } else {
    snprintf(buffer, sizeof(buffer), "%us", seconds);
  }
  return std::string(buffer);
}

text_sensor::TextSensor *UnhandledMeterTracker::get_id_sensor(const std::string &meter_id) {
  return this->id_sensors_[meter_id];
}

text_sensor::TextSensor *UnhandledMeterTracker::get_last_seen_sensor(const std::string &meter_id) {
  return this->last_seen_sensors_[meter_id];
}

text_sensor::TextSensor *UnhandledMeterTracker::get_rssi_sensor(const std::string &meter_id) {
  return this->rssi_sensors_[meter_id];
}

}  // namespace wmbus_unhandled
}  // namespace esphome
