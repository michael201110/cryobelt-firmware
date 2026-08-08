#include "fan_control.h"
#include "config.h"
#include <esp_arduino_version.h>

void FanControl::begin(uint8_t pwmPin, uint8_t currentPin) {
  pwmPin_ = pwmPin;
  currentPin_ = currentPin;

  pinMode(currentPin_, INPUT);
  analogReadResolution(12);

#if ESP_ARDUINO_VERSION_MAJOR >= 3
  pwmAttached_ = ledcAttach(pwmPin_, FAN_PWM_HZ, FAN_PWM_BITS);
#else
  pwmAttached_ = ledcSetup(pwmChannel_, FAN_PWM_HZ, FAN_PWM_BITS) > 0;
  if (pwmAttached_) ledcAttachPin(pwmPin_, pwmChannel_);
#endif

  if (!pwmAttached_) {
    Serial.println("[FAN] LEDC attach failed.");
    digitalWrite(pwmPin_, LOW);
    pinMode(pwmPin_, OUTPUT);
  }

  setPercent(0);
}

void FanControl::setPercent(int percent) {
  percent_ = constrain(percent, 0, 100);

  const uint32_t maxDuty = (1UL << FAN_PWM_BITS) - 1UL;
  const uint32_t duty = (maxDuty * static_cast<uint32_t>(percent_)) / 100UL;
  if (!pwmAttached_) {
    percent_ = 0;
    digitalWrite(pwmPin_, LOW);
    return;
  }

#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(pwmPin_, duty);
#else
  ledcWrite(pwmChannel_, duty);
#endif
}

float FanControl::currentAmps() const {
  // Average calibrated readings to reduce switching noise from the 25 kHz fan
  // gate. Hardware RC filtering is also present on Rev-A.
  uint32_t mv = 0;
  constexpr uint8_t samples = 8;
  for (uint8_t i = 0; i < samples; ++i) {
    mv += analogReadMilliVolts(currentPin_);
  }
  mv /= samples;
  float volts = static_cast<float>(mv) / 1000.0f;

  // INA180A3: Vout = I * Rshunt * Gain
  return volts / (FAN_SHUNT_OHMS * INA180_GAIN);
}
