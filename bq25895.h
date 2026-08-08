#pragma once
#include <Arduino.h>
#include <Wire.h>

class BQ25895 {
public:
  explicit BQ25895(TwoWire& wire, uint8_t address = 0x6A);

  bool begin();
  bool isPresent() const { return present_; }

  bool readRegister(uint8_t reg, uint8_t& value);
  bool writeRegister(uint8_t reg, uint8_t value);
  bool updateBits(uint8_t reg, uint8_t mask, uint8_t value);

  bool setChargingEnabled(bool enabled);
  bool setOTGEnabled(bool enabled);
  bool setPowerModes(bool chargingEnabled, bool otgEnabled);
  bool setILIMPinEnabled(bool enabled);
  bool setInputCurrentLimitmA(uint16_t milliamps);
  bool setChargeCurrentmA(uint16_t milliamps);
  bool setChargeVoltagemV(uint16_t millivolts);
  bool setPrechargeCurrentmA(uint16_t milliamps);
  bool setTerminationCurrentmA(uint16_t milliamps);
  bool disableAutonomousInputDetection();
  bool disableWatchdog();
  bool configureSafetyTimer();
  bool setInputVoltageLimitmV(uint16_t millivolts);
  bool enableContinuousADC();
  bool readBatteryMillivolts(uint16_t& millivolts);
  bool verifyConfiguration(bool chargingExpected, Stream* diagnostics = nullptr);
  bool readStatus(uint8_t& status, uint8_t& fault);

  void dumpRegisters(Stream& out);

private:
  TwoWire& wire_;
  uint8_t address_;
  bool present_ = false;
};
