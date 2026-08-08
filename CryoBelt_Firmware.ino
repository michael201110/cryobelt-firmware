/*
  CryoBelt Rev-A firmware skeleton
  Target: ESP32-S3-MINI-1-N8
  Framework: Arduino-ESP32 3.x

  Hardware pin map is taken from the CryoBelt KiCad netlist supplied by Michael.

  IMPORTANT:
  - BQ25895 ILIM is intentionally left open in hardware.
  - Firmware keeps charging disabled while configuring the charger, then disables
    ILIM control and explicitly authorises charging.
  - OTG is OFF by default.
  - Audio is intentionally disabled: the supplied Rev-A netlist ties GPIO18 /
    MAX98357A DIN to the DOUT of the final SK6805. See README.md.
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

BQ25895 charger(Wire);
SHT40Simple climate(Wire);
FanControl fans;
FUSB303GPIO usbRole;
Buttons buttons;

Adafruit_NeoPixel pixels(
  RGB_COUNT,
  PIN_RGB_DATA,
  NEO_GRB + NEO_KHZ800
);

static uint32_t lastTelemetryMs = 0;
static uint32_t lastButtonMs = 0;

static void setStatusColour(uint8_t r, uint8_t g, uint8_t b) {
  for (uint8_t i = 0; i < RGB_COUNT; ++i) {
    pixels.setPixelColor(i, pixels.Color(r, g, b));
  }
  pixels.show();
}

static void initialiseChargerSafely() {
  Serial.println("[BQ] Probing BQ25895...");

  if (!charger.begin()) {
    Serial.println("[BQ] NOT FOUND. Charging remains hardware-inhibited by open ILIM.");
    setStatusColour(32, 0, 0);
    return;
  }

  Serial.println("[BQ] Found.");

  // First action: explicitly turn both charger and BQ boost/OTG off.
  // Open ILIM prevents normal input current before firmware clears EN_ILIM.
  bool ok = true;
  ok &= charger.setChargingEnabled(false);
  ok &= charger.setOTGEnabled(false);

  // Set I2C input current limit while ILIM is still enabled/open.
  ok &= charger.setInputCurrentLimitmA(CHARGE_INPUT_LIMIT_MA);

  // Hand input-current control to I2C.
  ok &= charger.setILIMPinEnabled(false);

  // Keep charging off unless explicitly authorised below.
  if (ALLOW_CHARGING_AFTER_BOOT) {
    ok &= charger.setChargingEnabled(true);
  }

  if (ok) {
    Serial.printf("[BQ] Configured. Charge=%s, IINLIM=%u mA\n",
                  ALLOW_CHARGING_AFTER_BOOT ? "ON" : "OFF",
                  CHARGE_INPUT_LIMIT_MA);
    setStatusColour(0, 20, 0);
  } else {
    Serial.println("[BQ] Configuration failed. Charging left OFF.");
    charger.setChargingEnabled(false);
    setStatusColour(32, 0, 0);
  }
}

void setup() {
  // Native USB CDC is convenient on ESP32-S3 when USB D+/D- are wired to GPIO20/19.
  Serial.begin(115200);
  delay(800);

  Serial.println();
  Serial.println("================================");
  Serial.println("       CryoBelt Rev-A");
  Serial.println("================================");

  pinMode(PIN_12V_EN, OUTPUT);
  digitalWrite(PIN_12V_EN, LOW);

  pinMode(PIN_OTG_GATE, OUTPUT);
  digitalWrite(PIN_OTG_GATE, LOW);

  pinMode(PIN_BQ_INT_N, INPUT_PULLUP);

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, I2C_FREQUENCY_HZ);

  pixels.begin();
  pixels.clear();
  pixels.setBrightness(RGB_BRIGHTNESS);
  setStatusColour(0, 0, 12);

  buttons.begin();
  fans.begin(PIN_FAN_GATE, PIN_FAN_CURRENT);

  usbRole.begin(PIN_FUSB_ID, PIN_FUSB_OUT1, PIN_FUSB_OUT2);

  initialiseChargerSafely();

  if (climate.begin()) {
    Serial.println("[SHT40] Found.");
  } else {
    Serial.println("[SHT40] Not found (expected until hardware is assembled).");
  }

  fans.setPercent(DEFAULT_FAN_PERCENT);
  digitalWrite(PIN_12V_EN, DEFAULT_FAN_PERCENT > 0 ? HIGH : LOW);

  Serial.println("[BOOT] Firmware ready.");
}

void loop() {
  buttons.update();
  usbRole.update();

  // Simple UI:
  // UP   = +10% fan
  // DOWN = -10% fan
  // USER = fan off/on using default level
  if (millis() - lastButtonMs > 50) {
    if (buttons.upPressed()) {
      fans.setPercent(min(100, fans.percent() + 10));
      digitalWrite(PIN_12V_EN, fans.percent() > 0 ? HIGH : LOW);
      Serial.printf("[UI] Fan %d%%\n", fans.percent());
      lastButtonMs = millis();
    }

    if (buttons.downPressed()) {
      fans.setPercent(max(0, fans.percent() - 10));
      digitalWrite(PIN_12V_EN, fans.percent() > 0 ? HIGH : LOW);
      Serial.printf("[UI] Fan %d%%\n", fans.percent());
      lastButtonMs = millis();
    }

    if (buttons.userPressed()) {
      if (fans.percent() == 0) {
        fans.setPercent(DEFAULT_FAN_PERCENT);
        digitalWrite(PIN_12V_EN, HIGH);
      } else {
        fans.setPercent(0);
        digitalWrite(PIN_12V_EN, LOW);
      }
      Serial.printf("[UI] Fan %d%%\n", fans.percent());
      lastButtonMs = millis();
    }
  }

  if (millis() - lastTelemetryMs >= TELEMETRY_PERIOD_MS) {
    lastTelemetryMs = millis();

    float tempC = NAN;
    float humidity = NAN;
    bool shtOK = climate.read(tempC, humidity);

    Serial.printf("[TEL] Fan=%d%%  I_fan=%.3f A  USB=%s",
                  fans.percent(),
                  fans.currentAmps(),
                  usbRole.description());

    if (shtOK) {
      Serial.printf("  T=%.2f C  RH=%.1f%%", tempC, humidity);
    }

    if (charger.isPresent()) {
      Serial.printf("  BQ_INT=%d", digitalRead(PIN_BQ_INT_N));
    }

    Serial.println();
  }

  delay(2);
}
