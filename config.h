#pragma once
#include <Arduino.h>

constexpr uint32_t I2C_FREQUENCY_HZ = 400000;

// Start conservative. Increase after hardware bring-up and thermal checks.
constexpr uint16_t CHARGE_INPUT_LIMIT_MA = 500;

// Michael's intended behaviour: ESP32 must explicitly authorise charging.
// Set false during first bench bring-up if desired.
constexpr bool ALLOW_CHARGING_AFTER_BOOT = true;

constexpr uint8_t RGB_COUNT = 3;
constexpr uint8_t RGB_BRIGHTNESS = 32;

constexpr int DEFAULT_FAN_PERCENT = 40;
constexpr uint32_t FAN_PWM_HZ = 25000;
constexpr uint8_t FAN_PWM_BITS = 10;

constexpr uint32_t TELEMETRY_PERIOD_MS = 2000;

// Fan current-sense:
// Rshunt = 0.05 ohm
// INA180A3 gain = 100 V/V
constexpr float FAN_SHUNT_OHMS = 0.05f;
constexpr float INA180_GAIN = 100.0f;
