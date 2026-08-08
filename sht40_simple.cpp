#include "sht40_simple.h"

SHT40Simple::SHT40Simple(TwoWire& wire, uint8_t address)
  : wire_(wire), address_(address) {}

bool SHT40Simple::begin() {
  wire_.beginTransmission(address_);
  return wire_.endTransmission() == 0;
}

bool SHT40Simple::read(float& temperatureC, float& relativeHumidity) {
  // High precision measurement, no heater
  wire_.beginTransmission(address_);
  wire_.write(0xFD);
  if (wire_.endTransmission() != 0) return false;

  delay(10);

  if (wire_.requestFrom(address_, static_cast<uint8_t>(6)) != 6) return false;

  uint16_t rawT = (static_cast<uint16_t>(wire_.read()) << 8);
  rawT |= wire_.read();
  wire_.read(); // CRC byte (TODO: validate)

  uint16_t rawRH = (static_cast<uint16_t>(wire_.read()) << 8);
  rawRH |= wire_.read();
  wire_.read(); // CRC byte (TODO: validate)

  temperatureC = -45.0f + 175.0f * (static_cast<float>(rawT) / 65535.0f);
  relativeHumidity = -6.0f + 125.0f * (static_cast<float>(rawRH) / 65535.0f);

  if (relativeHumidity < 0.0f) relativeHumidity = 0.0f;
  if (relativeHumidity > 100.0f) relativeHumidity = 100.0f;

  return true;
}
