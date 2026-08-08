#include "sht40_simple.h"

static uint8_t sht40Crc(const uint8_t* data, size_t length) {
  uint8_t crc = 0xFF;
  for (size_t i = 0; i < length; ++i) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc & 0x80) ? static_cast<uint8_t>((crc << 1) ^ 0x31)
                         : static_cast<uint8_t>(crc << 1);
    }
  }
  return crc;
}

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

  uint8_t data[6];
  for (uint8_t& byte : data) byte = static_cast<uint8_t>(wire_.read());
  if (sht40Crc(&data[0], 2) != data[2] ||
      sht40Crc(&data[3], 2) != data[5]) {
    return false;
  }

  uint16_t rawT = (static_cast<uint16_t>(data[0]) << 8) | data[1];
  uint16_t rawRH = (static_cast<uint16_t>(data[3]) << 8) | data[4];

  temperatureC = -45.0f + 175.0f * (static_cast<float>(rawT) / 65535.0f);
  relativeHumidity = -6.0f + 125.0f * (static_cast<float>(rawRH) / 65535.0f);

  if (relativeHumidity < 0.0f) relativeHumidity = 0.0f;
  if (relativeHumidity > 100.0f) relativeHumidity = 100.0f;

  return true;
}
