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
  const int requestedPercent = constrain(percent, 0, 100);
  if (requestedPercent != percent_) {
    lastDutyChangeMs_ = millis();
  }
  percent_ = requestedPercent;

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

  // Production BOM: 10 mOhm R25 * INA180A3 gain 100 = 1 V/A.
  return volts / FAN_CURRENT_SENSE_VOLTS_PER_AMP;
}

bool FanControl::estimatePodCount(float measuredCurrentAmps,
                                  float supplyVoltage,
                                  uint8_t& count) const {
  count = 0;
  if (!pwmAttached_ || percent_ < POD_ESTIMATE_MIN_FAN_PERCENT ||
      millis() - lastDutyChangeMs_ < POD_ESTIMATE_SETTLE_MS ||
      measuredCurrentAmps < 0.0f || supplyVoltage <= 0.0f) {
    return false;
  }

  const float duty = static_cast<float>(percent_) / 100.0f;
  const float equivalentFullPowerW = supplyVoltage * measuredCurrentAmps / duty;
  const long estimate = lroundf(equivalentFullPowerW / POD_FAN_RATED_POWER_W);
  count = static_cast<uint8_t>(constrain(estimate, 0L,
                                         static_cast<long>(MAX_ESTIMATED_PODS)));
  return true;
}

bool FanControl::exceedsPodCurrentLimit(float measuredCurrentAmps) const {
  if (!pwmAttached_ || percent_ < POD_ESTIMATE_MIN_FAN_PERCENT ||
      measuredCurrentAmps < 0.0f) {
    return false;
  }

  const float duty = static_cast<float>(percent_) / 100.0f;
  const float equivalentFullCurrentA = measuredCurrentAmps / duty;
  const float sixPodRatedMaximumA =
    MAX_ALLOWED_PODS * POD_FAN_MAX_CURRENT_A + POD_CURRENT_LIMIT_MARGIN_A;
  return equivalentFullCurrentA > sixPodRatedMaximumA;
}

bool FanControl::exceedsAbsoluteCurrentLimit(float measuredCurrentAmps) const {
  if (!pwmAttached_ || measuredCurrentAmps < 0.0f) return false;
  const float sixPodRatedMaximumA =
    MAX_ALLOWED_PODS * POD_FAN_MAX_CURRENT_A + POD_CURRENT_LIMIT_MARGIN_A;
  return measuredCurrentAmps > sixPodRatedMaximumA;
}
