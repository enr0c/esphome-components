import os
from pathlib import Path

import esphome.config_validation as cv
from esphome import codegen as cg
from esphome.const import CONF_ID, SOURCE_FILE_EXTENSIONS
from esphome.core import LOGGER

CODEOWNERS = ["@SzczepanLeon", "@kubasaw"]
CONF_DRIVERS = "drivers"

wmbus_common_ns = cg.esphome_ns.namespace("wmbus_common")
WMBusCommon = wmbus_common_ns.class_("WMBusCommon", cg.Component)


AVAILABLE_DRIVERS = {
    f.stem.removeprefix("driver_") for f in Path(__file__).parent.glob("driver_*.cc")
}

# Allow loader/codegen to pick up C++ sources with .cc extension.
# (wmbus_common uses many .cc files; ESPHome defaults don't always include it.)
SOURCE_FILE_EXTENSIONS.add(".cc")


def _validate_drivers(value):
    if value == "all":
        return set(AVAILABLE_DRIVERS)
    return value


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(WMBusCommon),
        cv.Optional(CONF_DRIVERS, default=set()): cv.All(
            _validate_drivers,
            {cv.one_of(*AVAILABLE_DRIVERS, lower=True, space="_")},
        ),
    }
)


async def to_code(config):
    selected_drivers = set(config.get(CONF_DRIVERS, set()))

    if selected_drivers:
        selected = ",".join(sorted(selected_drivers))
        # Expose for C++ (defines.h) and for PlatformIO pre-build filtering.
        cg.add_define("ESPHOME_WMBUS_INCLUDE_DRIVERS", selected)
        cg.add_platformio_option("custom_wmbus_include_drivers", selected)
        LOGGER.info("wmbus_common: selected_drivers=%s", sorted(selected_drivers))

    # Pre-build script physically disables non-selected drivers in the build tree.
    script_path = os.path.join(os.path.dirname(__file__), "filter_wmbus_drivers.py")
    if os.path.exists(script_path):
        cg.add_platformio_option("extra_scripts", [f"pre:{script_path}"])

    var = cg.new_Pvariable(config[CONF_ID], sorted(selected_drivers))
    await cg.register_component(var, config)
