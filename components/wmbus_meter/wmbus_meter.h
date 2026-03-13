#pragma once
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include "esphome/components/time/real_time_clock.h"

#include "esphome/components/wmbus_common/meters.h"
#include "esphome/components/wmbus_radio/component.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

namespace esphome {
namespace wmbus_meter {
class Meter : public Component {
public:
  void set_meter_params(std::string id, std::string driver, std::string key,
                        std::initializer_list<LinkMode> linkModes);
  void set_radio(wmbus_radio::Radio *radio);

  void setup() override;
  void loop() override;
  void dump_config() override;
  std::string get_id();
  std::string get_driver();
  std::string get_key();

  void on_telegram(std::function<void()> &&callback);

  std::string as_json(bool pretty_print = false);
  optional<std::string> get_string_field(std::string field_name);
  optional<float> get_numeric_field(std::string field_name);

protected:
  LinkModeSet link_modes_;
  time::RealTimeClock *rtc;
  wmbus_radio::Radio *radio;

  std::shared_ptr<::Meter> meter;
  std::unique_ptr<Telegram> last_telegram;

  CallbackManager<void()> on_telegram_callback_manager;

  void handle_frame(wmbus_radio::Frame *frame);

  // Background parser task infrastructure.
  // ParseWork and ParseResult are heap-allocated; ownership is transferred
  // through the queues and explicitly freed by the consumer.
  struct ParseWork {
    std::vector<uchar> frame_data;
    std::string friendly_name;
    int8_t rssi;
    LinkMode lm;
  };

  struct ParseResult {
    Telegram *telegram; // heap-allocated; main loop must delete
    int8_t rssi;
    LinkMode lm;
  };

  QueueHandle_t parse_work_queue_{nullptr};
  QueueHandle_t parse_result_queue_{nullptr};
  // Binary semaphore: the parser task waits on this after posting a result to
  // ensure the main loop has finished reading ::Meter state before the parser
  // may start the next handleTelegram() write.
  SemaphoreHandle_t result_consumed_sem_{nullptr};
  TaskHandle_t parser_task_handle_{nullptr};

  static void parser_task(void *pvParameters);
};
} // namespace wmbus_meter
} // namespace esphome