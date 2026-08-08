#pragma once
#include <Arduino.h>

class FanControl {
public:
  void begin(uint8_t pwmPin, uint8_t currentPin);
  void setPercent(int percent);
  int percent() const { return percent_; }
  float currentAmps() const;

private:
  uint8_t pwmPin_ = 255;
  uint8_t currentPin_ = 255;
  uint8_t pwmChannel_ = 0;
  bool pwmAttached_ = false;
  int percent_ = 0;
};
