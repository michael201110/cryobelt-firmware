#pragma once
#include <Arduino.h>

class CryoBeltBLE {
public:
  static constexpr uint8_t PROTOCOL_VERSION = 1;
  static constexpr size_t TELEMETRY_SIZE = 16;

  enum class Opcode : uint8_t {
    SET_POWER = 1,
    SET_FAN_PERCENT = 2,
    SET_MODE = 3,
  };

  enum class Mode : uint8_t {
    MANUAL = 0,
    QUIET = 1,
    AUTO = 2,
    MAXIMUM = 3,
  };

  struct Command {
    Opcode opcode = Opcode::SET_POWER;
    uint8_t value = 0;
  };

  struct Telemetry {
    bool coolingEnabled;
    bool climateValid;
    bool chargerPresent;
    bool chargerConfigValid;
    bool chargingAuthorised;
    uint8_t requestedFanPercent;
    uint8_t actualFanPercent;
    Mode mode;
    uint8_t usbRole;
    float temperatureC;
    float humidityPercent;
    float fanCurrentAmps;
    uint16_t batteryMillivolts;
    uint8_t chargerStatus;
    uint8_t chargerFault;
  };

  bool begin();
  bool takeCommand(Command& command);
  bool takeDisconnected();
  void publish(const Telemetry& telemetry);
};
