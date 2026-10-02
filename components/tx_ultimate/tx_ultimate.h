#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include <vector>

namespace esphome {
namespace tx_ultimate {

// Defaults. Both are overridable from YAML - see __init__.py.
static const uint32_t DEFAULT_HOLD_TIMEOUT_MS = 500;
static const uint32_t DEFAULT_DOUBLE_TAP_WINDOW_MS = 200;

// Bytes needed to parse one packet: 4-byte header + event + release_pos + press_pos.
// Real hardware sends 8-9 bytes; trailing bytes are flushed in loop() after parsing.
static const uint8_t PACKET_MIN_LEN = 7;

// Special release-position codes reported by the hardware
static const uint8_t TWO_FINGER_POS = 0x0B;
static const uint8_t SWIPE_DOWN_POS  = 0x0C;
static const uint8_t SWIPE_UP_POS    = 0x0D;

// Touch strip reports finger positions 1..12 across its width.
static const uint8_t MAX_POSITION = 12;
static const uint8_t POSITIONS_PER_ZONE = 3;

struct ZoneState {
  uint32_t press_time{0};
  bool pressed{false};
  bool hold_fired{false};
  uint32_t last_tap_time{0};
  bool pending_tap{false};
};

class TxUltimate : public Component, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;

  // Called from to_code before automation triggers are wired; allocates
  // triggers and fills the default position ranges for each zone.
  void set_num_zones(uint8_t n);

  // 0 disables double-tap detection entirely, which also removes the window's
  // worth of latency from every single tap.
  void set_double_tap_window(uint32_t ms) { double_tap_window_ms_ = ms; }
  void set_hold_timeout(uint32_t ms) { hold_timeout_ms_ = ms; }

  // Which raw finger positions (1..12) belong to a zone. `zone` is 0-indexed.
  void set_zone_positions(uint8_t zone, uint8_t min_pos, uint8_t max_pos);

  Trigger<> *get_on_tap_trigger(uint8_t zone) { return on_tap_triggers_[zone]; }
  Trigger<> *get_on_hold_trigger(uint8_t zone) { return on_hold_triggers_[zone]; }
  Trigger<> *get_on_double_tap_trigger(uint8_t zone) { return on_double_tap_triggers_[zone]; }
  Trigger<> *get_on_swipe_up_trigger() { return &on_swipe_up_trigger_; }
  Trigger<> *get_on_swipe_down_trigger() { return &on_swipe_down_trigger_; }
  Trigger<> *get_on_two_finger_trigger() { return &on_two_finger_trigger_; }

  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  uint8_t num_zones_{4};
  uint32_t double_tap_window_ms_{DEFAULT_DOUBLE_TAP_WINDOW_MS};
  uint32_t hold_timeout_ms_{DEFAULT_HOLD_TIMEOUT_MS};

  std::vector<uint8_t> rx_buf_;
  std::vector<ZoneState> zone_states_;

  // Per-zone inclusive position ranges, parallel to zone_states_.
  std::vector<uint8_t> zone_min_pos_;
  std::vector<uint8_t> zone_max_pos_;

  std::vector<Trigger<> *> on_tap_triggers_;
  std::vector<Trigger<> *> on_hold_triggers_;
  std::vector<Trigger<> *> on_double_tap_triggers_;

  Trigger<> on_swipe_up_trigger_;
  Trigger<> on_swipe_down_trigger_;
  Trigger<> on_two_finger_trigger_;

  uint8_t active_press_zone_{0};  // 1-indexed; 0 = none

  uint8_t pos_to_zone(uint8_t pos);
  void handle_packet();
  void handle_press(uint8_t zone);
  void handle_release(uint8_t press_zone);
  void check_holds_and_double_taps();
};

}  // namespace tx_ultimate
}  // namespace esphome
