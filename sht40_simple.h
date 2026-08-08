#pragma once
#include <Arduino.h>
#include <Wire.h>

class SHT40Simple {
public:
  explicit SHT40Simple(TwoWire& wire, uint8_t address = 0x44);
  bool begin();
  bool read(float& temperatureC, float& relativeHumidity);

private:
  TwoWire& wire_;
  uint8_t address_;
};
