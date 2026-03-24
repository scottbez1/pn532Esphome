#pragma once

#include "esphome/components/spi/spi.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include <string>

namespace esphome {
namespace pn532_spi {

// PN532 over SPI (mode 0, LSB-first, max 5 MHz).
// No external Arduino library — the PN532 SPI framing protocol is
// implemented directly so there are zero lib_deps and no LDF issues.
class PN532SpiComponent
    : public PollingComponent,
      public spi::SPIDevice<spi::BIT_ORDER_LSB_FIRST, spi::CLOCK_POLARITY_LOW,
                            spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_4MHZ> {
 public:
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
  bool initialized_{false};
  bool tag_present_{false};
  std::string current_uid_;

  CallbackManager<void(std::string)> on_tag_callback_;
  CallbackManager<void()> on_tag_removed_callback_;

  // Low-level protocol helpers
  bool wait_ready_(uint16_t timeout_ms);
  bool write_command_(const uint8_t *data, uint8_t len);
  bool read_ack_();
  bool read_response_(uint8_t cmd, uint8_t *buf, uint8_t max_len, uint8_t &out_len);

  // High-level PN532 commands
  bool cmd_get_firmware_version_();
  bool cmd_sam_config_();
  bool cmd_set_max_retries_();
  bool cmd_read_passive_target_(uint8_t *uid, uint8_t &uid_len);

  static std::string uid_to_string_(const uint8_t *uid, uint8_t len);
};

class TagTrigger : public Trigger<std::string> {
 public:
  explicit TagTrigger(PN532SpiComponent *parent) {
    parent->add_on_tag_callback(
        [this](std::string uid) { this->trigger(std::move(uid)); });
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
