#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace tx_ultimate {

class TxUltimate : public Component, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;

  float get_setup_priority() const override { return setup_priority::DATA; }
};

}  // namespace tx_ultimate
}  // namespace esphome
