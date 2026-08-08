/*
  CryoBelt Rev-A firmware skeleton
  Target: ESP32-S3-MINI-1-N8
  Framework: Arduino-ESP32 2.x or 3.x

  Hardware pin map is taken from the CryoBelt KiCad netlist supplied by Michael.

  IMPORTANT:
  - BQ25895 ILIM is intentionally left open in hardware.
  - Firmware keeps charging disabled while configuring the charger, then disables
    ILIM control and explicitly authorises charging.
  - OTG is OFF by default.
  - This board has the Rev-A GPIO18 / final-SK6805-DOUT conflict repaired by
    cutting the D3 DOUT trace. Audio must not be enabled on an unreworked PCB.
*/

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>

#include "pins.h"
#include "config.h"
#include "bq25895.h"
#include "sht40_simple.h"
#include "fan_control.h"
#include "fusb303_gpio.h"
#include "buttons.h"
#include "cryobelt_ble.h"
#include "audio_output.h"

BQ25895 charger(Wire);
SHT40Simple climate(Wire);
FanControl fans;
FUSB303GPIO usbRole;
Buttons buttons;
CryoBeltBLE beltBLE;
AudioOutput audio;

Adafruit_NeoPixel pixels(
  RGB_COUNT,
  PIN_RGB_DATA,
  NEO_GRB + NEO_KHZ800
);

static uint32_t lastTelemetryMs = 0;
static uint32_t lastBleTelemetryMs = 0;
static uint32_t lastButtonMs = 0;
static bool chargingAuthorised = false;
static bool coolingEnabled = false;
static uint8_t requestedFanPercent = DEFAULT_FAN_PERCENT;
static CryoBeltBLE::Mode coolingMode = CryoBeltBLE::Mode::MANUAL;
static bool climateValid = false;
static float lastTemperatureC = NAN;
static float lastHumidityPercent = NAN;
static float lastFanCurrentAmps = 0.0f;
static bool chargerConfigValid = false;
static uint16_t batteryMillivolts = 0;
static uint8_t chargerStatus = 0;
static uint8_t chargerFault = 0;
static uint8_t statusRed = 0;
static uint8_t statusGreen = 0;
static uint8_t statusBlue = 0;
static bool findBeltActive = false;
static uint32_t findBeltStartedMs = 0;
static uint32_t findBeltLastStep = UINT32_MAX;

static void showPixels(uint8_t r, uint8_t g, uint8_t b) {
  for (uint8_t i = 0; i < RGB_COUNT; ++i) {
    pixels.setPixelColor(i, pixels.Color(r, g, b));
  }
  pixels.show();
}

static void setStatusColour(uint8_t r, uint8_t g, uint8_t b) {
  statusRed = r;
  statusGreen = g;
  statusBlue = b;
  showPixels(r, g, b);
}

static void stopFindBelt() {
  if (!findBeltActive) return;
  findBeltActive = false;
  showPixels(statusRed, statusGreen, statusBlue);
}

static void startFindBelt() {
  findBeltActive = true;
  findBeltStartedMs = millis();
  findBeltLastStep = UINT32_MAX;
  Serial.println("[FIND] Locator alert started.");
}

static void updateFindBelt() {
  if (!findBeltActive) return;

  const uint32_t elapsed = millis() - findBeltStartedMs;
  if (elapsed >= FIND_BELT_DURATION_MS) {
    stopFindBelt();
    Serial.println("[FIND] Locator alert finished.");
    return;
  }

  const uint32_t step = elapsed / FIND_BELT_FLASH_PERIOD_MS;
  if (step == findBeltLastStep) return;
  findBeltLastStep = step;

  if ((step & 1U) == 0) {
    showPixels(0, 80, 255);
    if ((step % 4U) == 0) {
      audio.playTone(1320, 55, FIND_BELT_VOLUME_PERCENT);
    }
  } else {
    showPixels(0, 0, 0);
  }
}

static uint8_t effectiveFanPercent() {
  if (!coolingEnabled) return 0;
  switch (coolingMode) {
    case CryoBeltBLE::Mode::QUIET:
      return requestedFanPercent < 30 ? requestedFanPercent : 30;
    case CryoBeltBLE::Mode::AUTO:
      return DEFAULT_FAN_PERCENT;
    case CryoBeltBLE::Mode::MAXIMUM:
      return 100;
    case CryoBeltBLE::Mode::MANUAL:
    default:
      return requestedFanPercent;
  }
}

static void applyCoolingOutput() {
  const uint8_t percent = effectiveFanPercent();
  fans.setPercent(percent);
  digitalWrite(PIN_12V_EN, percent > 0 ? HIGH : LOW);
}

static void handleBleCommands() {
  if (beltBLE.takeDisconnected()) {
    stopFindBelt();
    coolingEnabled = false;
    applyCoolingOutput();
    Serial.println("[BLE] Disconnected; cooling stopped.");
  }

  CryoBeltBLE::Command command;
  while (beltBLE.takeCommand(command)) {
    switch (command.opcode) {
      case CryoBeltBLE::Opcode::SET_POWER:
        coolingEnabled = command.value != 0;
        break;
      case CryoBeltBLE::Opcode::SET_FAN_PERCENT:
        requestedFanPercent = command.value;
        coolingMode = CryoBeltBLE::Mode::MANUAL;
        break;
      case CryoBeltBLE::Opcode::SET_MODE:
        coolingMode = static_cast<CryoBeltBLE::Mode>(command.value);
        break;
      case CryoBeltBLE::Opcode::FIND_BELT:
        startFindBelt();
        break;
    }
    applyCoolingOutput();
    Serial.printf("[BLE] Power=%s Fan=%u%% Mode=%u\n",
                  coolingEnabled ? "ON" : "OFF",
                  requestedFanPercent,
                  static_cast<unsigned>(coolingMode));
  }
}

static void publishBleTelemetry() {
  CryoBeltBLE::Telemetry telemetry = {
    coolingEnabled,
    climateValid,
    charger.isPresent(),
    chargerConfigValid,
    chargingAuthorised,
    requestedFanPercent,
    static_cast<uint8_t>(fans.percent()),
    coolingMode,
    static_cast<uint8_t>(usbRole.role()),
    lastTemperatureC,
    lastHumidityPercent,
    lastFanCurrentAmps,
    batteryMillivolts,
    chargerStatus,
    chargerFault,
  };
  beltBLE.publish(telemetry);
}

static void forceChargerSafe() {
  stopFindBelt();
  chargerConfigValid = false;
  // Best effort, deliberately ordered so the hardware ILIM clamp is restored
  // even if another register transaction fails.
  charger.setPowerModes(false, false);
  charger.setILIMPinEnabled(true);
  charger.disableAutonomousInputDetection();
  charger.disableWatchdog();
}

static void initialiseChargerSafely() {
  Serial.println("[BQ] Probing BQ25895...");

  if (!charger.begin()) {
    Serial.println("[BQ] NOT FOUND. Charging remains hardware-inhibited by open ILIM.");
    setStatusColour(32, 0, 0);
    return;
  }

  Serial.println("[BQ] Found.");

  // First establish the fail-closed state. The open ILIM pin clamps input
  // current while EN_ILIM remains enabled.
  bool ok = charger.setPowerModes(false, false);
  ok = charger.setILIMPinEnabled(true) && ok;

  if (!ok) {
    Serial.println("[BQ] Could not establish safe state.");
    forceChargerSafe();
    setStatusColour(32, 0, 0);
    return;
  }

  // D+/D- on the BQ25895 are unconnected on Rev-A. Disable autonomous input
  // negotiation and ICO so IINLIM remains the deterministic effective limit.
  ok = charger.disableAutonomousInputDetection();
  ok = charger.setInputCurrentLimitmA(CHARGE_INPUT_LIMIT_MA) && ok;
  ok = charger.setInputVoltageLimitmV(INPUT_VOLTAGE_LIMIT_MV) && ok;
  ok = charger.setChargeCurrentmA(CHARGE_CURRENT_MA) && ok;
  ok = charger.setChargeVoltagemV(CHARGE_VOLTAGE_MV) && ok;
  ok = charger.setPrechargeCurrentmA(PRECHARGE_CURRENT_MA) && ok;
  ok = charger.setTerminationCurrentmA(TERMINATION_CURRENT_MA) && ok;
  ok = charger.configureSafetyTimer() && ok;
  ok = charger.disableWatchdog() && ok;
  ok = charger.enableContinuousADC() && ok;

  if (!ok || !charger.verifyConfiguration(false, &Serial)) {
    Serial.println("[BQ] Configuration failed. Charging remains inhibited.");
    forceChargerSafe();
    setStatusColour(32, 0, 0);
    return;
  }

  chargingAuthorised = CHARGER_PROFILE_VALIDATED && ALLOW_CHARGING_AFTER_BOOT;
  if (chargingAuthorised) {
    // Remove the hardware clamp only after every setting has been verified.
    ok = charger.setILIMPinEnabled(false);
    ok = charger.setChargingEnabled(true) && ok;
    ok = ok && charger.verifyConfiguration(true, &Serial);
  }

  if (!ok) {
    Serial.println("[BQ] Charge authorisation failed. Restoring safe state.");
    chargingAuthorised = false;
    forceChargerSafe();
    setStatusColour(32, 0, 0);
    return;
  }

  chargerConfigValid = true;

  Serial.printf("[BQ] Configured. Charge=%s, IINLIM=%u mA, ICHG=%u mA\n",
                chargingAuthorised ? "ON" : "INHIBITED",
                CHARGE_INPUT_LIMIT_MA,
                CHARGE_CURRENT_MA);
  setStatusColour(chargingAuthorised ? 0 : 20,
                  chargingAuthorised ? 20 : 12,
                  0);
}

void setup() {
  // Establish safe control levels before USB startup, logging, or delays.
  digitalWrite(PIN_12V_EN, LOW);
  pinMode(PIN_12V_EN, OUTPUT);

  digitalWrite(PIN_OTG_GATE, LOW);
  pinMode(PIN_OTG_GATE, OUTPUT);

  digitalWrite(PIN_FAN_GATE, LOW);
  pinMode(PIN_FAN_GATE, OUTPUT);

  // Native USB CDC is convenient on ESP32-S3 when USB D+/D- are wired to GPIO20/19.
  Serial.begin(115200);

  Serial.println();
  Serial.println("================================");
  Serial.println("       CryoBelt Rev-A");
  Serial.println("================================");

  pinMode(PIN_BQ_INT_N, INPUT_PULLUP);

  Wire.setTimeOut(I2C_TIMEOUT_MS);
  if (!Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, I2C_FREQUENCY_HZ)) {
    Serial.println("[I2C] Bus initialisation failed.");
  }

  pixels.begin();
  pixels.clear();
  pixels.setBrightness(STATUS_LED_BRIGHTNESS);
  setStatusColour(0, 0, 12);

  initialiseChargerSafely();

  buttons.begin();
  fans.begin(PIN_FAN_GATE, PIN_FAN_CURRENT);

  usbRole.begin(PIN_FUSB_ID, PIN_FUSB_OUT1, PIN_FUSB_OUT2);

  if (AUDIO_HARDWARE_REWORKED && audio.begin()) {
    Serial.println("[AUDIO] MAX98357A I2S output enabled.");
    audio.playTone(880, 70, AUDIO_STARTUP_VOLUME_PERCENT);
    audio.playTone(1175, 90, AUDIO_STARTUP_VOLUME_PERCENT);
  } else {
    Serial.println("[AUDIO] Disabled or I2S initialisation failed.");
  }

  if (climate.begin()) {
    Serial.println("[SHT40] Found.");
  } else {
    Serial.println("[SHT40] Not found (expected until hardware is assembled).");
  }

  // Keep the boost and fan off until the user explicitly requests airflow.
  applyCoolingOutput();

  if (beltBLE.begin()) {
    Serial.println("[BLE] Advertising as CryoBelt.");
  } else {
    Serial.println("[BLE] Initialisation failed; local controls remain available.");
  }

  Serial.println("[BOOT] Firmware ready.");
}

void loop() {
  buttons.update();
  usbRole.update();
  handleBleCommands();
  updateFindBelt();

  // Simple UI:
  // UP   = +10% fan
  // DOWN = -10% fan
  // USER = fan off/on using default level
  if (millis() - lastButtonMs > 50) {
    if (buttons.upPressed()) {
      requestedFanPercent = requestedFanPercent <= 90
        ? requestedFanPercent + 10 : 100;
      coolingMode = CryoBeltBLE::Mode::MANUAL;
      coolingEnabled = requestedFanPercent > 0;
      applyCoolingOutput();
      Serial.printf("[UI] Fan %u%%\n", requestedFanPercent);
      lastButtonMs = millis();
    }

    if (buttons.downPressed()) {
      requestedFanPercent = requestedFanPercent >= 10
        ? requestedFanPercent - 10 : 0;
      coolingMode = CryoBeltBLE::Mode::MANUAL;
      coolingEnabled = requestedFanPercent > 0;
      applyCoolingOutput();
      Serial.printf("[UI] Fan %u%%\n", requestedFanPercent);
      lastButtonMs = millis();
    }

    if (buttons.userPressed()) {
      coolingEnabled = !coolingEnabled;
      applyCoolingOutput();
      Serial.printf("[UI] Fan %d%%\n", fans.percent());
      lastButtonMs = millis();
    }
  }

  if (millis() - lastTelemetryMs >= TELEMETRY_PERIOD_MS) {
    lastTelemetryMs = millis();

    climateValid = climate.read(lastTemperatureC, lastHumidityPercent);
    lastFanCurrentAmps = fans.currentAmps();

    Serial.printf("[TEL] Fan=%d%%  I_fan=%.3f A  USB=%s",
                  fans.percent(),
                  lastFanCurrentAmps,
                  usbRole.description());

    if (climateValid) {
      Serial.printf("  T=%.2f C  RH=%.1f%%", lastTemperatureC, lastHumidityPercent);
    }

    if (charger.isPresent()) {
      chargerConfigValid = charger.verifyConfiguration(chargingAuthorised, &Serial);
      if (!chargerConfigValid) {
        Serial.print("  BQ_CONFIG=INVALID");
        chargingAuthorised = false;
        forceChargerSafe();
        setStatusColour(32, 0, 0);
      } else if (charger.readStatus(chargerStatus, chargerFault)) {
        if (!charger.readBatteryMillivolts(batteryMillivolts)) {
          batteryMillivolts = 0;
        }
        Serial.printf("  BQ_STATUS=0x%02X  BQ_FAULT=0x%02X  VBAT=%u mV",
                      chargerStatus, chargerFault, batteryMillivolts);
      } else {
        Serial.print("  BQ_READ=ERROR");
      }
    }

    Serial.println();
  }

  if (millis() - lastBleTelemetryMs >= BLE_TELEMETRY_PERIOD_MS) {
    lastBleTelemetryMs = millis();
    publishBleTelemetry();
  }

  delay(2);
}
