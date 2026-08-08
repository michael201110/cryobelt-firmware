#include "buttons.h"
#include "pins.h"

void Buttons::begin() {
  pinMode(PIN_BUTTON_UP, INPUT_PULLUP);
  pinMode(PIN_BUTTON_USER, INPUT_PULLUP);
  pinMode(PIN_BUTTON_DOWN, INPUT_PULLUP);

  lastUp_ = digitalRead(PIN_BUTTON_UP) == LOW;
  lastUser_ = digitalRead(PIN_BUTTON_USER) == LOW;
  lastDown_ = digitalRead(PIN_BUTTON_DOWN) == LOW;
}

void Buttons::update() {
  if (millis() - lastDebounceMs_ < 15) return;
  lastDebounceMs_ = millis();

  bool upNow = digitalRead(PIN_BUTTON_UP) == LOW;
  bool userNow = digitalRead(PIN_BUTTON_USER) == LOW;
  bool downNow = digitalRead(PIN_BUTTON_DOWN) == LOW;

  if (upNow && !lastUp_) upEvent_ = true;
  if (userNow && !lastUser_) userEvent_ = true;
  if (downNow && !lastDown_) downEvent_ = true;

  lastUp_ = upNow;
  lastUser_ = userNow;
  lastDown_ = downNow;
}

bool Buttons::upPressed() {
  bool e = upEvent_;
  upEvent_ = false;
  return e;
}

bool Buttons::userPressed() {
  bool e = userEvent_;
  userEvent_ = false;
  return e;
}

bool Buttons::downPressed() {
  bool e = downEvent_;
  downEvent_ = false;
  return e;
}
