# esphome-tx-ultimate
ESPHome external component for the Sonoff TX Ultimate touch switch

## Setup

### 1. Reference the external component

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/MaestroEngineering/esphome-tx-ultimate
      ref: main
    components: [tx_ultimate]
```

### 2. Configure the UART bus

```yaml
uart:
  id: uart_bus
  tx_pin: GPIO19
  rx_pin: GPIO22
  baud_rate: 115200
```

### 3. Add the component with callbacks

```yaml
tx_ultimate:
  uart_id: uart_bus
  zones:
    - on_tap:
        - logger.log: "Zone 1 tapped"
      on_hold:
        - logger.log: "Zone 1 held"
      on_double_tap:
        - logger.log: "Zone 1 double-tapped"
    # add up to 4 zones …
  on_swipe_up:
    - logger.log: "Swipe up"
  on_swipe_down:
    - logger.log: "Swipe down"
  on_two_finger:
    - logger.log: "Two-finger gesture"
```

### Tuning the touch areas and tap latency

```yaml
tx_ultimate:
  uart_id: uart_bus
  double_tap_window: 0ms      # no double-tap, taps fire instantly
  zones:
    - min_position: 1         # shift the boundaries if presses land
      max_position: 2         # on the wrong button
      on_tap:
        - logger.log: "Zone 1 tapped"
    - min_position: 3
      max_position: 5
    - min_position: 6
      max_position: 8
    - min_position: 9
      max_position: 12
```

With `double_tap_window: 0ms` a tap fires the moment you lift your finger.
Any non-zero value forces every single tap to wait that long, because the
component cannot know a tap is single until the window has passed.

A complete example is in [tx_ultimate.yaml](tx_ultimate.yaml). Flash it with:

```bash
esphome run tx_ultimate.yaml
```

You will need a `secrets.yaml` alongside it:

```yaml
wifi_ssid: "YourSSID"
wifi_password: "YourPassword"
```

## YAML reference

### `tx_ultimate` block

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `uart_id` | id | required | UART bus defined above |
| `zones` | list | 4 empty zones | Per-zone automation callbacks (1–4 entries) |
| `double_tap_window` | time | `200ms` | How long a tap waits to see if a second one follows. **`0ms` disables double-tap detection**, and taps then fire on release with no delay. |
| `hold_timeout` | time | `500ms` | Press duration that counts as a hold |
| `on_swipe_up` | automation | — | Fired on an upward swipe (0x0D) |
| `on_swipe_down` | automation | — | Fired on a downward swipe (0x0C) |
| `on_two_finger` | automation | — | Fired on a two-finger touch |

### Per-zone callbacks

Each entry in the `zones` list supports:

| Key | Description |
|-----|-------------|
| `min_position` | First raw finger position (1–12) belonging to this zone |
| `max_position` | Last raw finger position (1–12) belonging to this zone |
| `on_tap` | Short press and release on the same zone |
| `on_hold` | Press held with no release for `hold_timeout` |
| `on_double_tap` | Two taps on the same zone within `double_tap_window` |

Zones are ordered left → right. You may define 1–4 zones; omitting `zones` entirely defaults to 4 zones with no callbacks.

## Protocol

The switch sends 7–9 byte packets over UART:

| Byte | Value | Meaning |
|------|-------|---------|
| 0–3 | `AA 55 01 02` | Fixed header |
| 4 | `0x02` / `0x01` | Event: press / release |
| 5 | position | Release finger position (or `0x0B` = two-finger) |
| 6 | position | Press finger position |

**Position → zone mapping** (positions 1–12 across the touch strip):

| Position | Zone |
|----------|------|
| 1–3 | Z1 |
| 4–6 | Z2 |
| 7–9 | Z3 |
| 10–12 | Z4 |

## Gesture detection

| Gesture | Condition |
|---------|-----------|
| Tap | Press and release on the same zone. Fires after `double_tap_window` expires, or immediately on release when that is `0ms` |
| Hold | Press with no release within `hold_timeout`; suppresses the subsequent tap |
| Double tap | Two complete tap cycles on the same zone within `double_tap_window` |
| Swipe up | Release position byte = `0x0D` |
| Swipe down | Release position byte = `0x0C` |
| Two-finger | Release position byte = `0x0B` |

Swipes are reported directly by the hardware as special position codes — the
component does not infer them by comparing press and release zones.

## Hardware

| Signal | GPIO |
|--------|------|
| UART TX | 19 |
| UART RX | 22 |
| Baud rate | 115200 |

## Component structure

```
components/tx_ultimate/
├── __init__.py       # ESPHome schema, code generation
├── tx_ultimate.h     # Class declaration, ZoneState, trigger getters
└── tx_ultimate.cpp   # UART parsing, packet handling, gesture detection
```
