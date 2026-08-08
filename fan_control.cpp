#include "fan_control.h"
#include "config.h"

void FanControl::begin(uint8_t pwmPin, uint8_t currentPin) {
  pwmPin_ = pwmPin;
  currentPin_ = currentPin;

  pinMode(currentPin_, INPUT);
  analogReadResolution(12);

  // Arduino-ESP32 3.x pin-based LEDC API
  if (!ledcAttach(pwmPin_, FAN_PWM_HZ, FAN_PWM_BITS)) {
    Serial.println("[FAN] LEDC attach failed.");
  }

  setPercent(0);
}

void FanControl::setPercent(int percent) {
  percent_ = constrain(percent, 0, 100);

  const uint32_t maxDuty = (1UL << FAN_PWM_BITS) - 1UL;
  const uint32_t duty = (maxDuty * static_cast<uint32_t>(percent_)) / 100UL;
  ledcWrite(pwmPin_, duty);
}

float FanControl::currentAmps() const {
  // ESP32 Arduino analogReadMilliVolts performs ADC calibration when supported.
  uint32_t mv = analogReadMilliVolts(currentPin_);
  float volts = static_cast<float>(mv) / 1000.0f;

  // INA180A3: Vout = I * Rshunt * Gain
  return volts / (FAN_SHUNT_OHMS * INA180_GAIN);
}
