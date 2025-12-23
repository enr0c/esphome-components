import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_ID

from ..wmbus_radio import RadioComponent

CONF_RADIO_ID = "radio_id"

CODEOWNERS = ["@enr0c"]
DEPENDENCIES = ["wmbus_radio"]
AUTO_LOAD = ["text_sensor"]

wmbus_unhandled_ns = cg.esphome_ns.namespace("wmbus_unhandled")
UnhandledMeterTracker = wmbus_unhandled_ns.class_("UnhandledMeterTracker", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(UnhandledMeterTracker),
        cv.GenerateID(CONF_RADIO_ID): cv.use_id(RadioComponent),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    
    radio = await cg.get_variable(config[CONF_RADIO_ID])
    cg.add(var.set_radio(radio))
