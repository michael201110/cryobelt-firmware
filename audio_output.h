#pragma once

#include <Arduino.h>

class AudioOutput {
public:
  bool begin();
  void playTone(uint16_t frequencyHz,
                uint16_t durationMs,
                uint8_t volumePercent);

private:
  bool ready_ = false;
};
