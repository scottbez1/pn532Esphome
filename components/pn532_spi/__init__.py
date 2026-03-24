import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins, automation
from esphome.const import (
    CONF_ID,
    CONF_CS_PIN,
    CONF_CLK_PIN,
    CONF_MISO_PIN,
    CONF_MOSI_PIN,
    CONF_TRIGGER_ID,
)

CODEOWNERS = []

pn532_spi_ns = cg.esphome_ns.namespace("pn532_spi")
PN532SpiComponent = pn532_spi_ns.class_("PN532SpiComponent", cg.PollingComponent)
TagTrigger = pn532_spi_ns.class_(
    "TagTrigger", automation.Trigger.template(cg.std_string)
)
TagRemovedTrigger = pn532_spi_ns.class_(
    "TagRemovedTrigger", automation.Trigger.template()
)

CONF_ON_TAG = "on_tag"
CONF_ON_TAG_REMOVED = "on_tag_removed"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(PN532SpiComponent),
        cv.Required(CONF_CS_PIN): pins.internal_gpio_output_pin_schema,
        cv.Required(CONF_CLK_PIN): pins.internal_gpio_output_pin_schema,
        cv.Required(CONF_MISO_PIN): pins.internal_gpio_input_pin_schema,
        cv.Required(CONF_MOSI_PIN): pins.internal_gpio_output_pin_schema,
        cv.Optional(CONF_ON_TAG): automation.validate_automation(
            {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(TagTrigger)}
        ),
        cv.Optional(CONF_ON_TAG_REMOVED): automation.validate_automation(
            {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(TagRemovedTrigger)}
        ),
    }
).extend(cv.polling_component_schema("150ms"))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cs_pin = await cg.gpio_pin_expression(config[CONF_CS_PIN])
    cg.add(var.set_cs_pin(cs_pin))

    clk_pin = await cg.gpio_pin_expression(config[CONF_CLK_PIN])
    cg.add(var.set_clk_pin(clk_pin))

    miso_pin = await cg.gpio_pin_expression(config[CONF_MISO_PIN])
    cg.add(var.set_miso_pin(miso_pin))

    mosi_pin = await cg.gpio_pin_expression(config[CONF_MOSI_PIN])
    cg.add(var.set_mosi_pin(mosi_pin))

    for conf in config.get(CONF_ON_TAG, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.std_string, "uid")], conf)

    for conf in config.get(CONF_ON_TAG_REMOVED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)

    # Seeed Studio PN532 Arduino library
    cg.add_library("https://github.com/Seeed-Studio/PN532", None)
