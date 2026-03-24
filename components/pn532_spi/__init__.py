import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import spi
from esphome.const import CONF_ID, CONF_TRIGGER_ID

DEPENDENCIES = ["spi"]
CODEOWNERS = []

pn532_spi_ns = cg.esphome_ns.namespace("pn532_spi")
PN532SpiComponent = pn532_spi_ns.class_(
    "PN532SpiComponent",
    cg.PollingComponent,
    spi.SPIDevice,
)
TagTrigger = pn532_spi_ns.class_(
    "TagTrigger", automation.Trigger.template(cg.std_string)
)
TagRemovedTrigger = pn532_spi_ns.class_(
    "TagRemovedTrigger", automation.Trigger.template()
)

CONF_ON_TAG = "on_tag"
CONF_ON_TAG_REMOVED = "on_tag_removed"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(PN532SpiComponent),
            cv.Optional(CONF_ON_TAG): automation.validate_automation(
                {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(TagTrigger)}
            ),
            cv.Optional(CONF_ON_TAG_REMOVED): automation.validate_automation(
                {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(TagRemovedTrigger)}
            ),
        }
    )
    .extend(cv.polling_component_schema("150ms"))
    .extend(spi.spi_device_schema())
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await spi.register_spi_device(var, config)

    for conf in config.get(CONF_ON_TAG, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.std_string, "uid")], conf)

    for conf in config.get(CONF_ON_TAG_REMOVED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
