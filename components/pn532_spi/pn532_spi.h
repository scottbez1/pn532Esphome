#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

// Seeed Studio PN532 library (https://github.com/Seeed-Studio/PN532)
#include <PN532/PN532.h>
#include <PN532_SPI/PN532_SPI.h>

#include <string>

namespace esphome {
namespace pn532_spi {

class PN532SpiComponent : public PollingComponent {
 public:
  void set_cs_pin(InternalGPIOPin *pin) { cs_pin_ = pin; }
  void set_clk_pin(InternalGPIOPin *pin) { clk_pin_ = pin; }
  void set_miso_pin(InternalGPIOPin *pin) { miso_pin_ = pin; }
  void set_mosi_pin(InternalGPIOPin *pin) { mosi_pin_ = pin; }

  void add_on_tag_callback(std::function<void(std::string)> callback) {
    on_tag_callback_.add(std::move(callback));
  }
  void add_on_tag_removed_callback(std::function<void()> callback) {
    on_tag_removed_callback_.add(std::move(callback));
  }

  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::LATE; }

 protected:
  InternalGPIOPin *cs_pin_{nullptr};
  InternalGPIOPin *clk_pin_{nullptr};
  InternalGPIOPin *miso_pin_{nullptr};
  InternalGPIOPin *mosi_pin_{nullptr};

  PN532_SPI *pn532_spi_{nullptr};
  PN532 *nfc_{nullptr};

  bool initialized_{false};
  bool tag_present_{false};
  std::string current_uid_;

  CallbackManager<void(std::string)> on_tag_callback_;
  CallbackManager<void()> on_tag_removed_callback_;

  static std::string uid_to_string(const uint8_t *uid, uint8_t length);
};

class TagTrigger : public Trigger<std::string> {
 public:
  explicit TagTrigger(PN532SpiComponent *parent) {
    parent->add_on_tag_callback([this](std::string uid) { this->trigger(std::move(uid)); });
  }
};

class TagRemovedTrigger : public Trigger<> {
 public:
  explicit TagRemovedTrigger(PN532SpiComponent *parent) {
    parent->add_on_tag_removed_callback([this]() { this->trigger(); });
  }
};

}  // namespace pn532_spi
}  // namespace esphome
