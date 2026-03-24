#include "pn532_spi.h"

#include <SPI.h>

namespace esphome {
namespace pn532_spi {

static const char *const TAG = "pn532_spi";

// Timeout (ms) passed to readPassiveTargetID. Caps how long each poll blocks.
static const uint16_t POLL_TIMEOUT_MS = 100;

void PN532SpiComponent::setup() {
  uint8_t cs = this->cs_pin_->get_pin();

#if defined(ARDUINO_ARCH_ESP32) || defined(ESP32)
  SPI.begin(
      static_cast<int>(this->clk_pin_->get_pin()),
      static_cast<int>(this->miso_pin_->get_pin()),
      static_cast<int>(this->mosi_pin_->get_pin()),
      static_cast<int>(cs));
#else
  SPI.begin();
#endif

  this->pn532_spi_ = new PN532_SPI(SPI, cs);
  this->nfc_ = new PN532(*this->pn532_spi_);

  this->nfc_->begin();

  uint32_t versiondata = this->nfc_->getFirmwareVersion();
  if (!versiondata) {
    ESP_LOGE(TAG, "PN532 not found — check wiring and CS pin");
    this->mark_failed();
    return;
  }

  ESP_LOGI(TAG, "Found PN5%02X, firmware %d.%d",
           (versiondata >> 24) & 0xFF,
           (versiondata >> 16) & 0xFF,
           (versiondata >> 8) & 0xFF);

  this->nfc_->SAMConfig();
  this->initialized_ = true;
}

void PN532SpiComponent::update() {
  if (!this->initialized_)
    return;

  uint8_t uid[7];
  uint8_t uid_length = 0;

  bool success = this->nfc_->readPassiveTargetID(
      PN532_MIFARE_ISO14443A, uid, &uid_length, POLL_TIMEOUT_MS);

  if (success && uid_length > 0) {
    std::string uid_str = uid_to_string(uid, uid_length);

    if (!this->tag_present_ || uid_str != this->current_uid_) {
      this->tag_present_ = true;
      this->current_uid_ = uid_str;
      ESP_LOGI(TAG, "Tag detected: %s", uid_str.c_str());
      this->on_tag_callback_.call(uid_str);
    }
  } else {
    if (this->tag_present_) {
      ESP_LOGI(TAG, "Tag removed: %s", this->current_uid_.c_str());
      this->tag_present_ = false;
      this->current_uid_.clear();
      this->on_tag_removed_callback_.call();
    }
  }
}

void PN532SpiComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "PN532 SPI:");
  LOG_PIN("  CS Pin:   ", this->cs_pin_);
  LOG_PIN("  CLK Pin:  ", this->clk_pin_);
  LOG_PIN("  MISO Pin: ", this->miso_pin_);
  LOG_PIN("  MOSI Pin: ", this->mosi_pin_);
  LOG_UPDATE_INTERVAL(this);
}

std::string PN532SpiComponent::uid_to_string(const uint8_t *uid, uint8_t length) {
  char buf[length * 3];
  char *p = buf;
  for (uint8_t i = 0; i < length; i++) {
    if (i > 0)
      *p++ = ':';
    p += sprintf(p, "%02X", uid[i]);
  }
  *p = '\0';
  return std::string(buf);
}

}  // namespace pn532_spi
}  // namespace esphome
