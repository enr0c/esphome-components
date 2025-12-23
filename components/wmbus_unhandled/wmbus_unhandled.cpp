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
  bool is_new = this->unhandled_meters_.find(meter_id) == this->unhandled_meters_.end();
  if (is_new) {
    ESP_LOGI(TAG, "New unhandled meter detected: %s (RSSI: %d dBm)", 
             meter_id.c_str(), rssi);
  }
  
  // Update meter info
  UnhandledMeterInfo &info = this->unhandled_meters_[meter_id];
  info.id = meter_id;
  info.rssi = rssi;
  info.last_seen_millis = now;
  info.last_seen = this->format_timestamp_(now);
  
  // Notify all registered callbacks
  this->notify_update_();
}

void UnhandledMeterTracker::notify_update_() {
  for (auto &callback : this->update_callbacks_) {
    callback();
  }
}

std::vector<std::string> UnhandledMeterTracker::get_meter_ids() const {
  std::vector<std::string> ids;
  for (const auto &pair : this->unhandled_meters_) {
    ids.push_back(pair.first);
  }
  return ids;
}

const UnhandledMeterInfo* UnhandledMeterTracker::get_meter_info(const std::string &meter_id) const {
  auto it = this->unhandled_meters_.find(meter_id);
  if (it != this->unhandled_meters_.end()) {
    return &it->second;
  }
  return nullptr;
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

// UnhandledMeterTextSensor implementation

void UnhandledMeterTextSensor::setup() {
  if (this->tracker_ == nullptr) {
    ESP_LOGE(TAG, "Tracker not set for text sensor!");
    this->mark_failed();
    return;
  }
  
  // Register callback to update when new data arrives
  this->tracker_->add_on_update_callback([this]() {
    this->update_sensor();
  });
}

void UnhandledMeterTextSensor::update_sensor() {
  auto meter_ids = this->tracker_->get_meter_ids();
  
  if (meter_ids.empty()) {
    this->publish_state("No unhandled meters");
    return;
  }
  
  // Publish count as state
  std::string state = std::to_string(meter_ids.size()) + " unhandled meter(s)";
  this->publish_state(state);
  
  // TODO: Add attributes with meter details when ESPHome supports it better
  // For now, log the details
  ESP_LOGD(TAG, "Unhandled meters:");
  for (const auto &id : meter_ids) {
    auto *info = this->tracker_->get_meter_info(id);
    if (info) {
      ESP_LOGD(TAG, "  %s: RSSI=%d dBm, Last seen=%s", 
               id.c_str(), info->rssi, info->last_seen.c_str());
    }
  }
}

}  // namespace wmbus_unhandled
}  // namespace esphome
