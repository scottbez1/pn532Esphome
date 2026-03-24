#include "pn532_spi.h"

#include "esphome/core/log.h"

namespace esphome {
namespace pn532_spi {

static const char *const TAG = "pn532_spi";

// ── SPI frame prefix bytes ────────────────────────────────────────────────────
static const uint8_t SPI_DATAWRITE = 0x01;
static const uint8_t SPI_STATREAD  = 0x02;
static const uint8_t SPI_DATAREAD  = 0x03;
static const uint8_t SPI_READY     = 0x01;

// ── PN532 frame constants ─────────────────────────────────────────────────────
static const uint8_t PREAMBLE    = 0x00;
static const uint8_t STARTCODE2  = 0xFF;
static const uint8_t POSTAMBLE   = 0x00;
static const uint8_t HOSTTOPN532 = 0xD4;
static const uint8_t PN532TOHOST = 0xD5;

// ── PN532 commands ────────────────────────────────────────────────────────────
static const uint8_t CMD_GETFIRMWAREVERSION  = 0x02;
static const uint8_t CMD_SAMCONFIGURATION    = 0x14;
static const uint8_t CMD_RFCONFIGURATION     = 0x32;
static const uint8_t CMD_INLISTPASSIVETARGET = 0x4A;

// ── Misc ──────────────────────────────────────────────────────────────────────
static const uint8_t BRTY_ISO14443A = 0x00;

// How long (ms) to wait for the PN532 status-ready bit after sending a command.
// With MaxRtyPassiveActivation=2 the chip responds within a few ms; 50 ms
// gives comfortable headroom without blocking the main loop for long.
static const uint16_t READY_TIMEOUT_MS = 50;

// ── Internal helpers ──────────────────────────────────────────────────────────

bool PN532SpiComponent::wait_ready_(uint16_t timeout_ms) {
  uint32_t start = millis();
  while (millis() - start < timeout_ms) {
    this->enable();
    this->transfer_byte(SPI_STATREAD);
    uint8_t status = this->transfer_byte(0x00);
    this->disable();
    if (status & SPI_READY)
      return true;
    delay(1);
  }
  return false;
}

bool PN532SpiComponent::write_command_(const uint8_t *data, uint8_t len) {
  // LEN covers TFI + all data bytes
  uint8_t frame_len = len + 1;
  uint8_t lcs       = static_cast<uint8_t>(~frame_len + 1);

  uint8_t checksum = HOSTTOPN532;
  for (uint8_t i = 0; i < len; i++)
    checksum += data[i];
  uint8_t dcs = static_cast<uint8_t>(~checksum + 1);

  this->enable();
  this->transfer_byte(SPI_DATAWRITE);
  this->transfer_byte(PREAMBLE);   // preamble
  this->transfer_byte(PREAMBLE);   // start code byte 1
  this->transfer_byte(STARTCODE2); // start code byte 2
  this->transfer_byte(frame_len);
  this->transfer_byte(lcs);
  this->transfer_byte(HOSTTOPN532);
  for (uint8_t i = 0; i < len; i++)
    this->transfer_byte(data[i]);
  this->transfer_byte(dcs);
  this->transfer_byte(POSTAMBLE);
  this->disable();
  return true;
}

bool PN532SpiComponent::read_ack_() {
  if (!this->wait_ready_(10))
    return false;

  // ACK frame: 00 00 FF 00 FF 00
  static const uint8_t ACK[6] = {0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00};
  this->enable();
  this->transfer_byte(SPI_DATAREAD);
  bool ok = true;
  for (uint8_t expected : ACK)
    if (this->transfer_byte(0x00) != expected)
      ok = false;
  this->disable();
  return ok;
}

bool PN532SpiComponent::read_response_(uint8_t cmd, uint8_t *buf, uint8_t max_len,
                                       uint8_t &out_len) {
  if (!this->wait_ready_(READY_TIMEOUT_MS))
    return false;

  this->enable();
  this->transfer_byte(SPI_DATAREAD);

  // Validate preamble + start code
  bool frame_ok = (this->transfer_byte(0) == PREAMBLE) &&
                  (this->transfer_byte(0) == PREAMBLE) &&
                  (this->transfer_byte(0) == STARTCODE2);

  uint8_t length = this->transfer_byte(0);
  uint8_t lcs    = this->transfer_byte(0);
  frame_ok &= ((uint8_t)(length + lcs) == 0x00);

  uint8_t tfi      = this->transfer_byte(0);
  uint8_t resp_cmd = this->transfer_byte(0);
  frame_ok &= (tfi == PN532TOHOST) && (resp_cmd == cmd + 1);

  if (!frame_ok) {
    this->disable();
    return false;
  }

  // length covers TFI (1) + resp_cmd (1) + data bytes
  uint8_t data_len = length - 2;
  uint8_t checksum = tfi + resp_cmd;
  out_len = 0;

  for (uint8_t i = 0; i < data_len; i++) {
    uint8_t b = this->transfer_byte(0);
    checksum += b;
    if (out_len < max_len)
      buf[out_len++] = b;
  }

  uint8_t dcs = this->transfer_byte(0);
  this->transfer_byte(0);  // postamble
  this->disable();

  return (uint8_t)(checksum + dcs) == 0x00;
}

// ── PN532 command wrappers ────────────────────────────────────────────────────

bool PN532SpiComponent::cmd_get_firmware_version_() {
  uint8_t cmd = CMD_GETFIRMWAREVERSION;
  if (!write_command_(&cmd, 1) || !read_ack_())
    return false;

  uint8_t resp[4];
  uint8_t resp_len;
  if (!read_response_(CMD_GETFIRMWAREVERSION, resp, sizeof(resp), resp_len) || resp_len < 4)
    return false;

  // resp[0]=IC, resp[1]=Ver, resp[2]=Rev, resp[3]=Support
  ESP_LOGI(TAG, "Found PN5%02X firmware v%d.%d", resp[0], resp[1], resp[2]);
  return true;
}

bool PN532SpiComponent::cmd_sam_config_() {
  // Normal mode, no timeout, use IRQ pin
  uint8_t cmd[] = {CMD_SAMCONFIGURATION, 0x01, 0x14, 0x01};
  if (!write_command_(cmd, sizeof(cmd)) || !read_ack_())
    return false;
  uint8_t resp[1];
  uint8_t resp_len;
  return read_response_(CMD_SAMCONFIGURATION, resp, sizeof(resp), resp_len);
}

bool PN532SpiComponent::cmd_set_max_retries_() {
  // MaxRtyATR=0xFF, MaxRtyPSL=0x01, MaxRtyPassiveActivation=0x02
  // With only 2 passive activation retries the chip responds in <5 ms
  // when no tag is present, keeping update() nearly non-blocking.
  uint8_t cmd[] = {CMD_RFCONFIGURATION, 0x05, 0xFF, 0x01, 0x02};
  if (!write_command_(cmd, sizeof(cmd)) || !read_ack_())
    return false;
  uint8_t resp[1];
  uint8_t resp_len;
  return read_response_(CMD_RFCONFIGURATION, resp, sizeof(resp), resp_len);
}

bool PN532SpiComponent::cmd_read_passive_target_(uint8_t *uid, uint8_t &uid_len) {
  uint8_t cmd[] = {CMD_INLISTPASSIVETARGET, 0x01, BRTY_ISO14443A};
  if (!write_command_(cmd, sizeof(cmd)) || !read_ack_())
    return false;

  uint8_t resp[20];
  uint8_t resp_len;
  if (!read_response_(CMD_INLISTPASSIVETARGET, resp, sizeof(resp), resp_len))
    return false;

  // resp[0]=NbTg, resp[1]=Tg, resp[2..3]=ATQA, resp[4]=SAK,
  // resp[5]=NfcIdLength, resp[6..]=NfcId
  if (resp_len < 7 || resp[0] == 0)
    return false;

  uid_len = resp[5];
  if (resp_len < static_cast<uint8_t>(6 + uid_len))
    return false;

  memcpy(uid, &resp[6], uid_len);
  return true;
}

// ── PollingComponent interface ────────────────────────────────────────────────

void PN532SpiComponent::setup() {
  this->spi_setup();

  // PN532 datasheet §7.2.1: after power-on the chip needs up to 400 ms before
  // its SPI interface is ready.  10 ms is too short on cold boot.
  delay(400);

  // Send a dummy SPI_STATREAD to clock the PN532's SPI state machine out of
  // any transient state it may be in after reset/power-on.
  this->enable();
  this->transfer_byte(SPI_STATREAD);
  this->transfer_byte(0x00);
  this->disable();
  delay(10);

  if (!cmd_get_firmware_version_()) {
    ESP_LOGE(TAG, "PN532 not found — check SPI wiring and that the PN532 is "
                  "strapped for SPI mode (SEL0=L, SEL1=H)");
    this->mark_failed();
    return;
  }
  if (!cmd_sam_config_()) {
    ESP_LOGE(TAG, "SAMConfig failed");
    this->mark_failed();
    return;
  }
  if (!cmd_set_max_retries_()) {
    ESP_LOGE(TAG, "RFConfiguration (MaxRetries) failed");
    this->mark_failed();
    return;
  }
  this->initialized_ = true;
}

void PN532SpiComponent::update() {
  if (!this->initialized_)
    return;

  uint8_t uid[7];
  uint8_t uid_len = 0;
  bool found = cmd_read_passive_target_(uid, uid_len);

  if (found && uid_len > 0) {
    std::string uid_str = uid_to_string_(uid, uid_len);
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
  LOG_UPDATE_INTERVAL(this);
}

// ── Utility ───────────────────────────────────────────────────────────────────

std::string PN532SpiComponent::uid_to_string_(const uint8_t *uid, uint8_t len) {
  char buf[len * 3];
  char *p = buf;
  for (uint8_t i = 0; i < len; i++) {
    if (i > 0)
      *p++ = ':';
    p += sprintf(p, "%02X", uid[i]);
  }
  *p = '\0';
  return std::string(buf);
}

}  // namespace pn532_spi
}  // namespace esphome
