#include "fusb303_gpio.h"

void FUSB303GPIO::begin(uint8_t idPin, uint8_t out1Pin, uint8_t out2Pin) {
  idPin_ = idPin;
  out1Pin_ = out1Pin;
  out2Pin_ = out2Pin;

  // FUSB303B GPIO outputs are open-drain in these modes, so internal pull-ups
  // provide readable HIGH when the output is high-Z.
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
    // OUT1=HIGH, OUT2=LOW also represents "no device attached".
    // Without an additional attach signal, keep that grouped with default/no-attach.
    if (!out1 &&  out2) role_ = Role::SINK_1_5A;
    else if (!out1 && !out2) role_ = Role::SINK_3A;
    else role_ = Role::NONE_OR_SINK_DEFAULT;
  }
}

const char* FUSB303GPIO::description() const {
  switch (role_) {
    case Role::NONE_OR_SINK_DEFAULT: return "none/sink-default";
    case Role::SINK_1_5A:            return "sink-1.5A";
    case Role::SINK_3A:              return "sink-3A";
    case Role::SOURCE_DEFAULT:       return "source-default";
    case Role::SOURCE_1_5A:          return "source-1.5A";
    case Role::SOURCE_3A:            return "source-3A";
    case Role::RESERVED:             return "reserved";
    default:                         return "?";
  }
}
