#include "tx_ultimate.h"
#include "esphome/core/log.h"

namespace esphome {
namespace tx_ultimate {

static const char *const TAG = "tx_ultimate";

static const uint8_t HEADER[] = {0xAA, 0x55, 0x01, 0x02};
static const uint8_t EVENT_PRESS = 0x02;
static const uint8_t EVENT_RELEASE = 0x01;
static const uint8_t EVENT_DRAGGED = 0x03;  // treated identically to release for swipe detection
static const uint8_t TWO_FINGER_POS = 0x0B;

// ── lifecycle ────────────────────────────────────────────────────────────────

void TxUltimate::set_num_zones(uint8_t n) {
  num_zones_ = n;
  while (on_tap_triggers_.size() < n) {
    on_tap_triggers_.push_back(new Trigger<>());
    on_hold_triggers_.push_back(new Trigger<>());
    on_double_tap_triggers_.push_back(new Trigger<>());
  }
}

void TxUltimate::setup() {
  ESP_LOGCONFIG(TAG, "TX Ultimate: %u zone(s)", num_zones_);
  // Ensure triggers exist even when set_num_zones wasn't called from YAML
  set_num_zones(num_zones_);
  zone_states_.resize(num_zones_);
}

// ── UART loop ────────────────────────────────────────────────────────────────

void TxUltimate::loop() {
  while (available()) {
    uint8_t b;
    read_byte(&b);
    rx_buf_.push_back(b);
  }

  while (rx_buf_.size() >= PACKET_MIN_LEN) {
    // Sync to 4-byte header
    if (rx_buf_[0] != HEADER[0] || rx_buf_[1] != HEADER[1] ||
        rx_buf_[2] != HEADER[2] || rx_buf_[3] != HEADER[3]) {
      ESP_LOGD(TAG, "Stray byte 0x%02X — resyncing", rx_buf_[0]);
      rx_buf_.erase(rx_buf_.begin());
      continue;
    }
    handle_packet();
    // Consume the 7 parsed bytes, then flush trailing packet bytes until the next 0xAA.
    rx_buf_.erase(rx_buf_.begin(), rx_buf_.begin() + PACKET_MIN_LEN);
    while (!rx_buf_.empty() && rx_buf_[0] != 0xAA)
      rx_buf_.erase(rx_buf_.begin());
  }

  check_holds_and_double_taps();
}

// ── packet parsing ───────────────────────────────────────────────────────────

uint8_t TxUltimate::pos_to_zone(uint8_t pos) {
  // Positions 1-3 = Z1, 4-6 = Z2, 7-9 = Z3, 10-12 = Z4
  if (pos == 0 || pos > 12) return 0;
  return (pos - 1) / 3 + 1;
}

void TxUltimate::handle_packet() {
  uint8_t event       = rx_buf_[4];
  uint8_t release_pos = rx_buf_[5];
  uint8_t press_pos   = rx_buf_[6];

  ESP_LOGD(TAG, "Packet event=0x%02X release_pos=0x%02X press_pos=0x%02X",
           event, release_pos, press_pos);

  if (event == EVENT_PRESS) {
    uint8_t zone = pos_to_zone(press_pos);
    if (zone >= 1 && zone <= num_zones_)
      handle_press(zone);
  } else if (event == EVENT_RELEASE || event == EVENT_DRAGGED) {
    if (release_pos == TWO_FINGER_POS) {
      ESP_LOGD(TAG, "Two-finger gesture");
      if (active_press_zone_ > 0 && active_press_zone_ <= num_zones_)
        zone_states_[active_press_zone_ - 1].pressed = false;
      on_two_finger_trigger_.trigger();
      active_press_zone_ = 0;
      return;
    }
    uint8_t release_zone = pos_to_zone(release_pos);
    handle_release(active_press_zone_, release_zone);
    active_press_zone_ = 0;
  }
}

// ── gesture handlers ─────────────────────────────────────────────────────────

void TxUltimate::handle_press(uint8_t zone) {
  ESP_LOGD(TAG, "Press zone %u", zone);
  active_press_zone_ = zone;
  ZoneState &s = zone_states_[zone - 1];
  s.pressed    = true;
  s.press_time = millis();
  s.hold_fired = false;
}

void TxUltimate::handle_release(uint8_t press_zone, uint8_t release_zone) {
  if (press_zone == 0 || press_zone > num_zones_) return;

  ZoneState &s = zone_states_[press_zone - 1];
  s.pressed = false;

  // Swipe: press and release on different zones
  if (release_zone != press_zone && release_zone >= 1 && release_zone <= num_zones_) {
    if (release_zone > press_zone) {
      ESP_LOGD(TAG, "Swipe right (Z%u → Z%u)", press_zone, release_zone);
      on_swipe_right_trigger_.trigger();
    } else {
      ESP_LOGD(TAG, "Swipe left (Z%u → Z%u)", press_zone, release_zone);
      on_swipe_left_trigger_.trigger();
    }
    return;
  }

  // Hold already fired — suppress tap
  if (s.hold_fired) return;

  // Tap / double-tap detection
  uint32_t now = millis();
  if (s.pending_tap && (now - s.last_tap_time) <= DOUBLE_TAP_WINDOW_MS) {
    ESP_LOGD(TAG, "Double tap zone %u", press_zone);
    s.pending_tap = false;
    on_double_tap_triggers_[press_zone - 1]->trigger();
  } else {
    s.last_tap_time = now;
    s.pending_tap   = true;
    // Single tap fires after the window expires (see check_holds_and_double_taps)
  }
}

void TxUltimate::check_holds_and_double_taps() {
  uint32_t now = millis();
  for (uint8_t i = 0; i < num_zones_; i++) {
    ZoneState &s = zone_states_[i];

    if (s.pressed && !s.hold_fired && (now - s.press_time) >= HOLD_TIMEOUT_MS) {
      ESP_LOGD(TAG, "Hold zone %u", i + 1);
      s.hold_fired = true;
      on_hold_triggers_[i]->trigger();
    }

    if (s.pending_tap && !s.pressed && (now - s.last_tap_time) > DOUBLE_TAP_WINDOW_MS) {
      ESP_LOGD(TAG, "Tap zone %u", i + 1);
      s.pending_tap = false;
      on_tap_triggers_[i]->trigger();
    }
  }
}

}  // namespace tx_ultimate
}  // namespace esphome
