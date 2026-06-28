# esphome-tx-ultimate
ESPHome external component for the Sonoff TX Ultimate touch switch

## Setup

### 1. Reference the external component

Add the following to your ESPHome YAML:

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/MaestroEngineering/esphome-tx-ultimate
      ref: main
    components: [tx_ultimate]
```

### 2. Configure the UART bus

The TX Ultimate communicates over UART on GPIO19 (TX) and GPIO22 (RX) at 115200 baud:

```yaml
uart:
  id: uart_bus
  tx_pin: GPIO19
  rx_pin: GPIO22
  baud_rate: 115200
```

### 3. Add the component

```yaml
tx_ultimate:
  uart_id: uart_bus
```

### 4. Enable debug logging (optional)

To see raw UART bytes in the logs, set the logger level to `DEBUG`:

```yaml
logger:
  level: DEBUG
```

## Example configuration

A complete working configuration is provided in [tx_ultimate.yaml](tx_ultimate.yaml).

Flash it with:

```bash
esphome run tx_ultimate.yaml
```

You will need a `secrets.yaml` file alongside it:

```yaml
wifi_ssid: "YourSSID"
wifi_password: "YourPassword"
```

## Component structure

```
components/tx_ultimate/
├── __init__.py       # ESPHome component registration
├── tx_ultimate.h     # C++ class declaration
└── tx_ultimate.cpp   # UART read loop, logs every byte as hex
```

## Hardware

| Signal | GPIO |
|--------|------|
| UART TX | 19 |
| UART RX | 22 |
| Baud rate | 115200 |
