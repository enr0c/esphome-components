#pragma once

#include "esphome/core/component.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/wmbus_radio/component.h"
#include "esphome/components/wmbus_radio/packet.h"
#include "esphome/components/wmbus_common/wmbus.h"
#include <map>
#include <string>

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
  
  text_sensor::TextSensor *get_id_sensor(const std::string &meter_id);
  text_sensor::TextSensor *get_last_seen_sensor(const std::string &meter_id);
  sensor::Sensor *get_rssi_sensor(const std::string &meter_id);
  
 protected:
  void handle_frame_(wmbus_radio::Frame *frame);
  void create_sensors_for_meter_(const std::string &meter_id);
  std::string format_timestamp_(uint32_t millis);
  
  wmbus_radio::Radio *radio_{nullptr};
  std::map<std::string, UnhandledMeterInfo> unhandled_meters_;
  std::map<std::string, text_sensor::TextSensor *> id_sensors_;
  std::map<std::string, text_sensor::TextSensor *> last_seen_sensors_;
  std::map<std::string, sensor::Sensor *> rssi_sensors_;
};

}  // namespace wmbus_unhandled
}  // namespace esphome
