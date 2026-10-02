#include "tx_ultimate.h"
#include "esphome/core/log.h"

namespace esphome {
namespace tx_ultimate {

static const char *const TAG = "tx_ultimate";

static const uint8_t HEADER[] = {0xAA, 0x55, 0x01, 0x02};
static const uint8_t EVENT_PRESS   = 0x02;
static const uint8_t EVENT_RELEASE = 0x01;
static const uint8_t EVENT_DRAGGED = 0x03;

// ── lifecycle ────────────────────────────────────────────────────────────────

void TxUltimate::set_num_zones(uint8_t n) {
  num_zones_ = n;
  while (on_tap_triggers_.size() < n) {
    on_tap_triggers_.push_back(new Trigger<>());
    on_hold_triggers_.push_back(new Trigger<>());
    on_double_tap_triggers_.push_back(new Trigger<>());
  }
  // Default split: zone 1 = 1-3, zone 2 = 4-6, zone 3 = 7-9, zone 4 = 10-12.
  // YAML can override any of these with min_position / max_position.
  zone_min_pos_.resize(n);
  zone_max_pos_.resize(n);
  for (uint8_t i = 0; i < n; i++) {
    zone_min_pos_[i] = i * POSITIONS_PER_ZONE + 1;
    zone_max_pos_[i] = i * POSITIONS_PER_ZONE + POSITIONS_PER_ZONE;
  }
}

void TxUltimate::set_zone_positions(uint8_t zone, uint8_t min_pos, uint8_t max_pos) {
  if (zone >= zone_min_pos_.size()) return;
  zone_min_pos_[zone] = min_pos;
  zone_max_pos_[zone] = max_pos;
}

void TxUltimate::setup() {
  // Ensure triggers and position ranges exist even when set_num_zones
  // wasn't called from YAML. This must run before the ranges are logged.
  if (zone_min_pos_.empty())
    set_num_zones(num_zones_);
  zone_states_.resize(num_zones_);

  ESP_LOGCONFIG(TAG, "TX Ultimate: %u zone(s)", num_zones_);
  if (double_tap_window_ms_ == 0) {
    ESP_LOGCONFIG(TAG, "  double tap: disabled (taps fire immediately)");
  } else {
    ESP_LOGCONFIG(TAG, "  double tap window: %ums", double_tap_window_ms_);
  }
  ESP_LOGCONFIG(TAG, "  hold timeout: %ums", hold_timeout_ms_);
  for (uint8_t i = 0; i < num_zones_; i++) {
    ESP_LOGCONFIG(TAG, "  zone %u: positions %u-%u", i + 1, zone_min_pos_[i], zone_max_pos_[i]);
  }
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
  if (pos == 0 || pos > MAX_POSITION) return 0;
  for (uint8_t i = 0; i < num_zones_ && i < zone_min_pos_.size(); i++) {
    if (pos >= zone_min_pos_[i] && pos <= zone_max_pos_[i])
      return i + 1;
  }
  // Position fell in a gap between configured ranges - ignore it rather than
  // guessing, so a deliberate dead band between buttons stays dead.
  return 0;
}

void TxUltimate::handle_packet() {
  uint8_t event       = rx_buf_[4];
  uint8_t release_pos = rx_buf_[5];
  uint8_t press_pos   = rx_buf_[6];

  ESP_LOGD(TAG, "Packet event=0x%02X release_pos=0x%02X press_pos=0x%02X",
           event, release_pos, press_pos);

  if (event == EVENT_PRESS) {
    uint8_t zone = pos_to_zone(press_pos);
    if (zone >= 1 && zone <= num_zones_) {
      handle_press(zone);
    } else {
      ESP_LOGD(TAG, "Position %u is not in any zone — ignored", press_pos);
    }
  } else if (event == EVENT_RELEASE || event == EVENT_DRAGGED) {
    // Special hardware-reported positions: handle before zone mapping
    if (release_pos == TWO_FINGER_POS || release_pos == SWIPE_DOWN_POS || release_pos == SWIPE_UP_POS) {
      if (active_press_zone_ > 0 && active_press_zone_ <= num_zones_) {
        ZoneState &s = zone_states_[active_press_zone_ - 1];
        s.pressed = false;
        // Clear any tap waiting out the double-tap window, otherwise a swipe
        // that follows a tap fires a spurious tap once the window expires.
        s.pending_tap = false;
      }
      if (release_pos == TWO_FINGER_POS) {
        ESP_LOGD(TAG, "Two-finger gesture");
        on_two_finger_trigger_.trigger();
      } else if (release_pos == SWIPE_DOWN_POS) {
        ESP_LOGD(TAG, "Swipe down");
        on_swipe_down_trigger_.trigger();
      } else {
        ESP_LOGD(TAG, "Swipe up");
        on_swipe_up_trigger_.trigger();
      }
      active_press_zone_ = 0;
      return;
    }
    handle_release(active_press_zone_);
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

void TxUltimate::handle_release(uint8_t press_zone) {
  if (press_zone == 0 || press_zone > num_zones_) return;

  ZoneState &s = zone_states_[press_zone - 1];
  s.pressed = false;

  // Hold already fired — suppress tap
  if (s.hold_fired) return;

  // Double-tap disabled: nothing to wait for, so fire the tap now. This is
  // what removes the window's worth of latency from every press.
  if (double_tap_window_ms_ == 0) {
    ESP_LOGD(TAG, "Tap zone %u", press_zone);
    on_tap_triggers_[press_zone - 1]->trigger();
    return;
  }

  // Tap / double-tap detection
  uint32_t now = millis();
  if (s.pending_tap && (now - s.last_tap_time) <= double_tap_window_ms_) {
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

    if (s.pressed && !s.hold_fired && (now - s.press_time) >= hold_timeout_ms_) {
      ESP_LOGD(TAG, "Hold zone %u", i + 1);
      s.hold_fired = true;
      on_hold_triggers_[i]->trigger();
    }

    if (s.pending_tap && !s.pressed && (now - s.last_tap_time) > double_tap_window_ms_) {
      ESP_LOGD(TAG, "Tap zone %u", i + 1);
      s.pending_tap = false;
      on_tap_triggers_[i]->trigger();
    }
  }
}

}  // namespace tx_ultimate
}  // namespace esphome
