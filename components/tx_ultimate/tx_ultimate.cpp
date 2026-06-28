#include "tx_ultimate.h"
#include "esphome/core/log.h"

namespace esphome {
namespace tx_ultimate {

static const char *const TAG = "tx_ultimate";

void TxUltimate::setup() {
  ESP_LOGCONFIG(TAG, "Setting up TX Ultimate...");
}

void TxUltimate::loop() {
  while (available()) {
    uint8_t byte;
    read_byte(&byte);
    ESP_LOGD(TAG, "RX byte: 0x%02X", byte);
  }
}

}  // namespace tx_ultimate
}  // namespace esphome
