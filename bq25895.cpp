#include "bq25895.h"
#include "config.h"

// BQ25895 register addresses used here
static constexpr uint8_t REG00_INPUT_SOURCE = 0x00;
static constexpr uint8_t REG02_ADC_CONTROL  = 0x02;
static constexpr uint8_t REG03_POWER_ON     = 0x03;
static constexpr uint8_t REG04_CHARGE_CURRENT = 0x04;
static constexpr uint8_t REG05_PRE_TERM     = 0x05;
static constexpr uint8_t REG06_CHARGE_VOLTAGE = 0x06;
static constexpr uint8_t REG07_TIMER_CONTROL = 0x07;
static constexpr uint8_t REG0B_STATUS       = 0x0B;
static constexpr uint8_t REG0C_FAULT        = 0x0C;
static constexpr uint8_t REG0D_VINDPM       = 0x0D;
static constexpr uint8_t REG14_PART_INFO    = 0x14;

static constexpr uint8_t REG00_EN_ILIM      = 0x40; // bit 6
static constexpr uint8_t REG00_IINLIM_MASK  = 0x3F; // bits 5:0

static constexpr uint8_t REG03_OTG_CONFIG   = 0x20; // bit 5
static constexpr uint8_t REG03_CHG_CONFIG   = 0x10; // bit 4

static constexpr uint8_t REG02_AUTONOMOUS_MASK = 0x1F; // ICO, HVDCP, MAXC, FORCE/AUTO_DPDM
static constexpr uint8_t REG02_CONV_RATE       = 0x40;
static constexpr uint8_t REG04_ICHG_MASK       = 0x7F;
static constexpr uint8_t REG05_IPRECHG_MASK    = 0xF0;
static constexpr uint8_t REG05_ITERM_MASK      = 0x0F;
static constexpr uint8_t REG06_VREG_MASK       = 0xFC;
static constexpr uint8_t REG07_WATCHDOG_MASK   = 0x30;
static constexpr uint8_t REG07_SAFETY_MASK     = 0x8E;
static constexpr uint8_t REG07_SAFETY_5_HOUR   = 0x88;
static constexpr uint8_t REG0D_FORCE_VINDPM    = 0x80;
static constexpr uint8_t REG0D_VINDPM_MASK     = 0x7F;
static constexpr uint8_t REG14_PN_MASK         = 0x38;
static constexpr uint8_t REG14_BQ25895_PN      = 0x38;

static uint8_t inputCurrentCode(uint16_t milliamps) {
  milliamps = constrain(milliamps, 100, 3250);
  return static_cast<uint8_t>((milliamps - 100) / 50);
}

static uint8_t chargeCurrentCode(uint16_t milliamps) {
  if (milliamps > 5056) milliamps = 5056;
  return static_cast<uint8_t>(milliamps / 64);
}

static uint8_t preTermCurrentCode(uint16_t milliamps) {
  milliamps = constrain(milliamps, 64, 1024);
  return static_cast<uint8_t>((milliamps - 64) / 64);
}

static uint8_t chargeVoltageCode(uint16_t millivolts) {
  millivolts = constrain(millivolts, 3840, 4608);
  return static_cast<uint8_t>((millivolts - 3840) / 16);
}

static uint8_t inputVoltageCode(uint16_t millivolts) {
  millivolts = constrain(millivolts, 3900, 15300);
  return static_cast<uint8_t>((millivolts - 2600) / 100);
}

BQ25895::BQ25895(TwoWire& wire, uint8_t address)
  : wire_(wire), address_(address) {}

bool BQ25895::begin() {
  present_ = false;
  for (uint8_t attempt = 0; attempt < 3; ++attempt) {
    uint8_t value = 0;
    if (readRegister(REG14_PART_INFO, value)) {
      present_ = (value & REG14_PN_MASK) == REG14_BQ25895_PN;
      break;
    }
    delay(2);
  }
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

bool BQ25895::setPowerModes(bool chargingEnabled, bool otgEnabled) {
  return updateBits(REG03_POWER_ON,
                    static_cast<uint8_t>(REG03_CHG_CONFIG | REG03_OTG_CONFIG),
                    static_cast<uint8_t>((chargingEnabled ? REG03_CHG_CONFIG : 0) |
                                         (otgEnabled ? REG03_OTG_CONFIG : 0)));
}

bool BQ25895::setILIMPinEnabled(bool enabled) {
  return updateBits(REG00_INPUT_SOURCE,
                    REG00_EN_ILIM,
                    enabled ? REG00_EN_ILIM : 0);
}

bool BQ25895::setInputCurrentLimitmA(uint16_t milliamps) {
  return updateBits(REG00_INPUT_SOURCE, REG00_IINLIM_MASK,
                    inputCurrentCode(milliamps));
}

bool BQ25895::setChargeCurrentmA(uint16_t milliamps) {
  return updateBits(REG04_CHARGE_CURRENT, REG04_ICHG_MASK,
                    chargeCurrentCode(milliamps));
}

bool BQ25895::setChargeVoltagemV(uint16_t millivolts) {
  return updateBits(REG06_CHARGE_VOLTAGE, REG06_VREG_MASK,
                    chargeVoltageCode(millivolts) << 2);
}

bool BQ25895::setPrechargeCurrentmA(uint16_t milliamps) {
  return updateBits(REG05_PRE_TERM, REG05_IPRECHG_MASK,
                    preTermCurrentCode(milliamps) << 4);
}

bool BQ25895::setTerminationCurrentmA(uint16_t milliamps) {
  return updateBits(REG05_PRE_TERM, REG05_ITERM_MASK,
                    preTermCurrentCode(milliamps));
}

bool BQ25895::disableAutonomousInputDetection() {
  return updateBits(REG02_ADC_CONTROL, REG02_AUTONOMOUS_MASK, 0);
}

bool BQ25895::disableWatchdog() {
  return updateBits(REG07_TIMER_CONTROL, REG07_WATCHDOG_MASK, 0);
}

bool BQ25895::configureSafetyTimer() {
  // Termination and the safety timer enabled, five-hour fast-charge timeout.
  return updateBits(REG07_TIMER_CONTROL, REG07_SAFETY_MASK,
                    REG07_SAFETY_5_HOUR);
}

bool BQ25895::setInputVoltageLimitmV(uint16_t millivolts) {
  return updateBits(REG0D_VINDPM,
                    static_cast<uint8_t>(REG0D_FORCE_VINDPM |
                                         REG0D_VINDPM_MASK),
                    static_cast<uint8_t>(REG0D_FORCE_VINDPM |
                                         inputVoltageCode(millivolts)));
}

bool BQ25895::enableContinuousADC() {
  return updateBits(REG02_ADC_CONTROL, REG02_CONV_RATE, REG02_CONV_RATE);
}

bool BQ25895::readBatteryMillivolts(uint16_t& millivolts) {
  uint8_t value = 0;
  if (!readRegister(0x0E, value)) return false;
  millivolts = static_cast<uint16_t>(2304 + 20 * (value & 0x7F));
  return true;
}

bool BQ25895::readStatus(uint8_t& status, uint8_t& fault) {
  return readRegister(REG0B_STATUS, status) && readRegister(REG0C_FAULT, fault);
}

bool BQ25895::verifyConfiguration(bool chargingExpected, Stream* diagnostics) {
  struct Check {
    uint8_t reg;
    uint8_t mask;
    uint8_t expected;
  };

  const Check checks[] = {
    {REG00_INPUT_SOURCE,
     static_cast<uint8_t>(REG00_EN_ILIM | REG00_IINLIM_MASK),
     static_cast<uint8_t>((chargingExpected ? 0 : REG00_EN_ILIM) |
                          inputCurrentCode(CHARGE_INPUT_LIMIT_MA))},
    {REG02_ADC_CONTROL, REG02_AUTONOMOUS_MASK, 0},
    {REG02_ADC_CONTROL, REG02_CONV_RATE, REG02_CONV_RATE},
    {REG03_POWER_ON,
     static_cast<uint8_t>(REG03_OTG_CONFIG | REG03_CHG_CONFIG),
     static_cast<uint8_t>(chargingExpected ? REG03_CHG_CONFIG : 0)},
    {REG04_CHARGE_CURRENT, REG04_ICHG_MASK,
     chargeCurrentCode(CHARGE_CURRENT_MA)},
    {REG05_PRE_TERM,
     static_cast<uint8_t>(REG05_IPRECHG_MASK | REG05_ITERM_MASK),
     static_cast<uint8_t>((preTermCurrentCode(PRECHARGE_CURRENT_MA) << 4) |
                          preTermCurrentCode(TERMINATION_CURRENT_MA))},
    {REG06_CHARGE_VOLTAGE, REG06_VREG_MASK,
     static_cast<uint8_t>(chargeVoltageCode(CHARGE_VOLTAGE_MV) << 2)},
    {REG07_TIMER_CONTROL, REG07_WATCHDOG_MASK, 0},
    {REG07_TIMER_CONTROL, REG07_SAFETY_MASK, REG07_SAFETY_5_HOUR},
    {REG0D_VINDPM,
     static_cast<uint8_t>(REG0D_FORCE_VINDPM | REG0D_VINDPM_MASK),
     static_cast<uint8_t>(REG0D_FORCE_VINDPM |
                          inputVoltageCode(INPUT_VOLTAGE_LIMIT_MV))},
  };

  for (const Check& check : checks) {
    uint8_t actual = 0;
    if (!readRegister(check.reg, actual) ||
        (actual & check.mask) != check.expected) {
      if (diagnostics) {
        diagnostics->printf("[BQ] Verify failed: REG%02X actual=0x%02X mask=0x%02X expected=0x%02X\n",
                            check.reg, actual, check.mask, check.expected);
      }
      return false;
    }
  }
  return true;
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
