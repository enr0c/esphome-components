#include "esphome/core/component.h"
#include "esphome/core/log.h"

#include "_version.h"
#include "driver_registry.h"

namespace esphome {
namespace wmbus_common {
static const char *TAG = "wmbus_common";
class WMBusCommon : public Component {
public:
  WMBusCommon(std::vector<std::string> drivers) : drivers_(drivers) {}
  void setup() override {
    // Ensure all driver translation units are linked in so their static
    // self-registration initializers have a chance to run before any meter
    // is created.  The individual anchor functions are empty; the only
    // purpose of this call is to create a reference chain that the linker
    // cannot eliminate.
    wmbus_ensure_all_drivers();
  }
  void dump_config() override {
    ESP_LOGCONFIG(TAG, "wM-Bus Component v%s-%s:", WMBUS_COMPONENT_VERSION,
                  WMBUSMETERS_VERSION);
    ESP_LOGCONFIG(TAG, "  Loaded drivers:");
    for (const auto &driver : this->drivers_)
      ESP_LOGCONFIG(TAG, "   %s", driver.c_str());
  }

protected:
  std::vector<std::string> drivers_;
};
} // namespace wmbus_common
} // namespace esphome