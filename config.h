#pragma once
#include <Arduino.h>

constexpr uint32_t I2C_FREQUENCY_HZ = 400000;
constexpr uint16_t I2C_TIMEOUT_MS = 50;

// Charger input limit. This is not the battery fast-charge current.
constexpr uint16_t CHARGE_INPUT_LIMIT_MA = 500;
constexpr uint16_t INPUT_VOLTAGE_LIMIT_MV = 4400;

// Validated battery envelope: single-cell 3.7 V nominal Li-ion, 4000 mAh,
// manufacturer charge-current limit 1C (4 A). Initial firmware charge current
// is deliberately limited to 960 mA (0.24C) for thermal and hardware bring-up.
// The BQ25895 quantises requested values down to supported register steps.
constexpr uint16_t CHARGE_CURRENT_MA = 960;
constexpr uint16_t CHARGE_VOLTAGE_MV = 4200;
constexpr uint16_t PRECHARGE_CURRENT_MA = 128;
constexpr uint16_t TERMINATION_CURRENT_MA = 128;

constexpr bool CHARGER_PROFILE_VALIDATED = true;
constexpr bool ALLOW_CHARGING_AFTER_BOOT = false;

constexpr uint8_t RGB_COUNT = 3;
constexpr uint8_t STATUS_LED_BRIGHTNESS = 32;

constexpr int DEFAULT_FAN_PERCENT = 40;
constexpr uint32_t FAN_PWM_HZ = 25000;
constexpr uint8_t FAN_PWM_BITS = 10;

// One pod contains one CFM-5010V-155-310 fan. Manufacturer ratings are
// 12 V, 81 mA maximum, and 0.98 W. The +12 V fan rail is not measured on
// Rev-A, so the estimator uses its nominal regulated voltage.
constexpr float FAN_SUPPLY_VOLTAGE_V = 12.0f;
constexpr float POD_FAN_RATED_POWER_W = 0.98f;
constexpr float POD_FAN_MAX_CURRENT_A = 0.081f;
constexpr uint8_t POD_ESTIMATE_MIN_FAN_PERCENT = 40;
constexpr uint32_t POD_ESTIMATE_SETTLE_MS = 750;
constexpr uint8_t MAX_ESTIMATED_PODS = 8;
constexpr uint8_t MAX_ALLOWED_PODS = 6;
constexpr uint32_t POD_RECHECK_DURATION_MS = 1000;
constexpr uint32_t POD_CHECK_SAMPLE_INTERVAL_MS = 50;
constexpr uint32_t POD_PERIODIC_RECHECK_MS = 30000;
constexpr float POD_CURRENT_LIMIT_MARGIN_A = 0.010f;

constexpr uint32_t TELEMETRY_PERIOD_MS = 2000;
constexpr uint32_t BLE_TELEMETRY_PERIOD_MS = 1000;

// Rev-A rework verified by continuity test: GPIO18 remains connected to the
// MAX98357A DIN input and is isolated from the final SK6805 DOUT.
constexpr bool AUDIO_HARDWARE_REWORKED = true;
constexpr uint32_t AUDIO_SAMPLE_RATE_HZ = 16000;
constexpr uint8_t AUDIO_STARTUP_VOLUME_PERCENT = 10;
constexpr uint32_t FIND_BELT_DURATION_MS = 8000;
constexpr uint32_t FIND_BELT_FLASH_PERIOD_MS = 250;
constexpr uint8_t FIND_BELT_VOLUME_PERCENT = 18;

// Fan current-sense:
// Rshunt = 0.05 ohm
// INA180A3 gain = 100 V/V
constexpr float FAN_SHUNT_OHMS = 0.05f;
constexpr float INA180_GAIN = 100.0f;
