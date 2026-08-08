#include "fusb303_gpio.h"

void FUSB303GPIO::begin(uint8_t idPin, uint8_t out1Pin, uint8_t out2Pin) {
  idPin_ = idPin;
  out1Pin_ = out1Pin;
  out2Pin_ = out2Pin;

  // ID and the sink-mode OUT signals are open-drain. Rev-A has no external
  // pulls on these three nets, so the ESP pull-ups are required. In source
  // mode OUT1/OUT2 become inputs; the same pulls select default source current.
  pinMode(idPin_, INPUT_PULLUP);
  pinMode(out1Pin_, INPUT_PULLUP);
  pinMode(out2Pin_, INPUT_PULLUP);

  update();
}

void FUSB303GPIO::update() {
  const bool id   = digitalRead(idPin_);
  const bool out1 = digitalRead(out1Pin_);
  const bool out2 = digitalRead(out2Pin_);

  // FUSB303B GPIO-mode truth table.
  // ID LOW = source role. ID high-Z (read HIGH) = sink/no-device family.
  if (!id) {
    if ( out1 &&  out2) role_ = Role::SOURCE_DEFAULT;
    else if (!out1 &&  out2) role_ = Role::SOURCE_1_5A;
    else if (!out1 && !out2) role_ = Role::SOURCE_3A;
    else role_ = Role::RESERVED;
  } else {
    if ( out1 && !out2) role_ = Role::NONE;
    else if (out1 && out2) role_ = Role::SINK_DEFAULT;
    else if (!out1 &&  out2) role_ = Role::SINK_1_5A;
    else if (!out1 && !out2) role_ = Role::SINK_3A;
    else role_ = Role::RESERVED;
  }
}

const char* FUSB303GPIO::description() const {
  switch (role_) {
    case Role::NONE:                 return "none";
    case Role::SINK_DEFAULT:         return "sink-default";
    case Role::SINK_1_5A:            return "sink-1.5A";
    case Role::SINK_3A:              return "sink-3A";
    case Role::SOURCE_DEFAULT:       return "source-default";
    case Role::SOURCE_1_5A:          return "source-1.5A";
    case Role::SOURCE_3A:            return "source-3A";
    case Role::RESERVED:             return "reserved";
    default:                         return "?";
  }
}
