#pragma once

#include "esphome/core/component.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/wmbus_radio/component.h"
#include "esphome/components/wmbus_radio/packet.h"
#include "esphome/components/wmbus_common/wmbus.h"
#include <map>
#include <string>
#include <vector>

namespace esphome {
namespace wmbus_unhandled {

struct UnhandledMeterInfo {
  std::string id;
  std::string last_seen;
  int rssi;
  uint32_t last_seen_millis;
};

class UnhandledMeterTracker : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  
  void set_radio(wmbus_radio::Radio *radio) { this->radio_ = radio; }
  
  // Get unhandled meter information
  std::vector<std::string> get_meter_ids() const;
  const UnhandledMeterInfo* get_meter_info(const std::string &meter_id) const;
  const std::map<std::string, UnhandledMeterInfo>& get_all_meters() const { return this->unhandled_meters_; }
  
  // Register a listener for updates
  void add_on_update_callback(std::function<void()> &&callback) {
    this->update_callbacks_.push_back(std::move(callback));
  }
  
 protected:
  void handle_frame_(wmbus_radio::Frame *frame);
  std::string format_timestamp_(uint32_t millis);
  void notify_update_();
  
  wmbus_radio::Radio *radio_{nullptr};
  std::map<std::string, UnhandledMeterInfo> unhandled_meters_;
  std::vector<std::function<void()>> update_callbacks_;
};

class UnhandledMeterTextSensor : public text_sensor::TextSensor, public Component {
 public:
  void set_tracker(UnhandledMeterTracker *tracker) { this->tracker_ = tracker; }
  void setup() override;
  void update_sensor();
  
 protected:
  UnhandledMeterTracker *tracker_{nullptr};
};

}  // namespace wmbus_unhandled
}  // namespace esphome
