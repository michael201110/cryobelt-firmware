#pragma once
#include <Arduino.h>

class FUSB303GPIO {
public:
  enum class Role {
    NONE,
    SINK_DEFAULT,
    SINK_1_5A,
    SINK_3A,
    SOURCE_DEFAULT,
    SOURCE_1_5A,
    SOURCE_3A,
    RESERVED
  };

  void begin(uint8_t idPin, uint8_t out1Pin, uint8_t out2Pin);
  void update();
  Role role() const { return role_; }
  const char* description() const;

private:
  uint8_t idPin_ = 255;
  uint8_t out1Pin_ = 255;
  uint8_t out2Pin_ = 255;
  Role role_ = Role::NONE;
};
