#pragma once
#include <Arduino.h>

class Buttons {
public:
  void begin();
  void update();

  bool upPressed();
  bool userPressed();
  bool downPressed();

private:
  bool readPressed(uint8_t pin, bool& lastState, bool& event);

  bool lastUp_ = false;
  bool lastUser_ = false;
  bool lastDown_ = false;

  bool upEvent_ = false;
  bool userEvent_ = false;
  bool downEvent_ = false;

  uint32_t lastDebounceMs_ = 0;
};
