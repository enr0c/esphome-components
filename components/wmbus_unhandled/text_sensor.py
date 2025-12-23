import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.components import text_sensor
from esphome.const import (
    CONF_ID,
    CONF_ICON,
)

from . import UnhandledMeterTracker, wmbus_unhandled_ns

CONF_TRACKER_ID = "tracker_id"

UnhandledMeterTextSensor = wmbus_unhandled_ns.class_(
    "UnhandledMeterTextSensor", text_sensor.TextSensor, cg.Component
)

CONFIG_SCHEMA = text_sensor.text_sensor_schema(
    UnhandledMeterTextSensor,
    icon="mdi:alert-circle-outline",
).extend(
    {
        cv.GenerateID(CONF_TRACKER_ID): cv.use_id(UnhandledMeterTracker),
    }
)


async def to_code(config):
    var = await text_sensor.new_text_sensor(config)
    await cg.register_component(var, config)
    
    tracker = await cg.get_variable(config[CONF_TRACKER_ID])
    cg.add(var.set_tracker(tracker))
