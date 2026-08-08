#include "bq25895.h"

// BQ25895 register addresses used here
static constexpr uint8_t REG00_INPUT_SOURCE = 0x00;
static constexpr uint8_t REG03_POWER_ON     = 0x03;

static constexpr uint8_t REG00_EN_ILIM      = 0x40; // bit 6
static constexpr uint8_t REG00_IINLIM_MASK  = 0x3F; // bits 5:0

static constexpr uint8_t REG03_WD_RST       = 0x40; // bit 6
static constexpr uint8_t REG03_OTG_CONFIG   = 0x20; // bit 5
static constexpr uint8_t REG03_CHG_CONFIG   = 0x10; // bit 4

BQ25895::BQ25895(TwoWire& wire, uint8_t address)
  : wire_(wire), address_(address) {}

bool BQ25895::begin() {
  uint8_t value = 0;
  present_ = readRegister(0x14, value); // part-information register
  return present_;
}

bool BQ25895::readRegister(uint8_t reg, uint8_t& value) {
  wire_.beginTransmission(address_);
  wire_.write(reg);
  if (wire_.endTransmission(false) != 0) return false;

  if (wire_.requestFrom(address_, static_cast<uint8_t>(1)) != 1) return false;
  value = wire_.read();
  return true;
}

bool BQ25895::writeRegister(uint8_t reg, uint8_t value) {
  wire_.beginTransmission(address_);
  wire_.write(reg);
  wire_.write(value);
  return wire_.endTransmission() == 0;
}

bool BQ25895::updateBits(uint8_t reg, uint8_t mask, uint8_t value) {
  uint8_t current = 0;
  if (!readRegister(reg, current)) return false;
  current = (current & ~mask) | (value & mask);
  return writeRegister(reg, current);
}

bool BQ25895::setChargingEnabled(bool enabled) {
  return updateBits(REG03_POWER_ON,
                    REG03_CHG_CONFIG,
                    enabled ? REG03_CHG_CONFIG : 0);
}

bool BQ25895::setOTGEnabled(bool enabled) {
  return updateBits(REG03_POWER_ON,
                    REG03_OTG_CONFIG,
                    enabled ? REG03_OTG_CONFIG : 0);
}

bool BQ25895::setILIMPinEnabled(bool enabled) {
  return updateBits(REG00_INPUT_SOURCE,
                    REG00_EN_ILIM,
                    enabled ? REG00_EN_ILIM : 0);
}

bool BQ25895::setInputCurrentLimitmA(uint16_t milliamps) {
  // Datasheet encoding:
  // 100 mA offset, 50 mA/LSB, 0..63 => 100 mA..3250 mA
  if (milliamps < 100) milliamps = 100;
  if (milliamps > 3250) milliamps = 3250;

  uint8_t code = static_cast<uint8_t>((milliamps - 100 + 25) / 50);
  if (code > 63) code = 63;

  return updateBits(REG00_INPUT_SOURCE, REG00_IINLIM_MASK, code);
}

bool BQ25895::resetWatchdog() {
  return updateBits(REG03_POWER_ON, REG03_WD_RST, REG03_WD_RST);
}

void BQ25895::dumpRegisters(Stream& out) {
  for (uint8_t reg = 0x00; reg <= 0x14; ++reg) {
    uint8_t value = 0;
    if (readRegister(reg, value)) {
      out.printf("BQ[0x%02X] = 0x%02X\n", reg, value);
    } else {
      out.printf("BQ[0x%02X] = READ ERROR\n", reg);
    }
  }
}
