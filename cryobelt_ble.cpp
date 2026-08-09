#include "cryobelt_ble.h"

#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>

namespace {

constexpr char DEVICE_NAME[] = "CryoBelt";
constexpr char SERVICE_UUID[] = "7d4b1000-6c4a-4f65-9f09-8a2c7d3e1000";
constexpr char COMMAND_UUID[] = "7d4b1001-6c4a-4f65-9f09-8a2c7d3e1000";
constexpr char TELEMETRY_UUID[] = "7d4b1002-6c4a-4f65-9f09-8a2c7d3e1000";

portMUX_TYPE stateMux = portMUX_INITIALIZER_UNLOCKED;
BLECharacteristic* telemetryCharacteristic = nullptr;
volatile uint8_t pendingOpcode = 0;
volatile uint8_t pendingValue = 0;
volatile bool clientConnected = false;
volatile bool clientDisconnected = false;

class CommandCallbacks final : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* characteristic) override {
    const auto value = characteristic->getValue();
    if (value.length() != 3 ||
        static_cast<uint8_t>(value[0]) != CryoBeltBLE::PROTOCOL_VERSION) {
      return;
    }

    const uint8_t opcode = static_cast<uint8_t>(value[1]);
    const uint8_t commandValue = static_cast<uint8_t>(value[2]);
    const bool valid =
      (opcode == static_cast<uint8_t>(CryoBeltBLE::Opcode::SET_POWER) &&
       commandValue <= 1) ||
      (opcode == static_cast<uint8_t>(CryoBeltBLE::Opcode::SET_FAN_PERCENT) &&
       commandValue <= 100) ||
      (opcode == static_cast<uint8_t>(CryoBeltBLE::Opcode::SET_MODE) &&
       commandValue <= 3) ||
      (opcode == static_cast<uint8_t>(CryoBeltBLE::Opcode::FIND_BELT) &&
       commandValue == 1) ||
      (opcode == static_cast<uint8_t>(CryoBeltBLE::Opcode::RECHECK_PODS) &&
       commandValue == 1);
    if (!valid) return;

    portENTER_CRITICAL(&stateMux);
    pendingValue = commandValue;
    pendingOpcode = opcode;
    portEXIT_CRITICAL(&stateMux);
  }
};

class ServerCallbacks final : public BLEServerCallbacks {
  void onConnect(BLEServer*) override {
    clientConnected = true;
  }

  void onDisconnect(BLEServer*) override {
    portENTER_CRITICAL(&stateMux);
    clientConnected = false;
    clientDisconnected = true;
    pendingOpcode = 0;
    portEXIT_CRITICAL(&stateMux);
    BLEDevice::startAdvertising();
  }
};

CommandCallbacks commandCallbacks;
ServerCallbacks serverCallbacks;

} // namespace

bool CryoBeltBLE::begin() {
  BLEDevice::init(DEVICE_NAME);
  BLEServer* server = BLEDevice::createServer();
  if (!server) return false;
  server->setCallbacks(&serverCallbacks);

  BLEService* service = server->createService(SERVICE_UUID);
  if (!service) return false;

  BLECharacteristic* commandCharacteristic = service->createCharacteristic(
    COMMAND_UUID,
    BLECharacteristic::PROPERTY_WRITE
  );
  telemetryCharacteristic = service->createCharacteristic(
    TELEMETRY_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
  );
  if (!commandCharacteristic || !telemetryCharacteristic) return false;

  commandCharacteristic->setCallbacks(&commandCallbacks);
  telemetryCharacteristic->addDescriptor(new BLE2902());

  uint8_t initial[TELEMETRY_SIZE] = {};
  initial[0] = PROTOCOL_VERSION;
  telemetryCharacteristic->setValue(initial, sizeof(initial));

  service->start();
  BLEAdvertising* advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(SERVICE_UUID);
  advertising->setScanResponse(true);
  advertising->setMinPreferred(0x06);
  advertising->setMaxPreferred(0x12);
  BLEDevice::startAdvertising();
  return true;
}

bool CryoBeltBLE::takeCommand(Command& command) {
  portENTER_CRITICAL(&stateMux);
  const uint8_t opcode = pendingOpcode;
  if (opcode != 0) {
    command.opcode = static_cast<Opcode>(opcode);
    command.value = pendingValue;
    pendingOpcode = 0;
  }
  portEXIT_CRITICAL(&stateMux);
  return opcode != 0;
}

bool CryoBeltBLE::takeDisconnected() {
  portENTER_CRITICAL(&stateMux);
  const bool disconnected = clientDisconnected;
  clientDisconnected = false;
  portEXIT_CRITICAL(&stateMux);
  return disconnected;
}

void CryoBeltBLE::publish(const Telemetry& telemetry) {
  if (!telemetryCharacteristic) return;

  uint8_t packet[TELEMETRY_SIZE] = {};
  packet[0] = PROTOCOL_VERSION;
  packet[1] =
    (telemetry.coolingEnabled ? 0x01 : 0) |
    (telemetry.climateValid ? 0x02 : 0) |
    (telemetry.chargerPresent ? 0x04 : 0) |
    (telemetry.chargerConfigValid ? 0x08 : 0) |
    (telemetry.chargingAuthorised ? 0x10 : 0) |
    (telemetry.podEstimateValid ? 0x20 : 0) |
    (telemetry.podSafetyLatched ? 0x40 : 0) |
    (telemetry.podCheckActive ? 0x80 : 0);
  packet[2] = constrain(telemetry.requestedFanPercent, 0, 100);
  packet[3] = constrain(telemetry.actualFanPercent, 0, 100);
  packet[4] = static_cast<uint8_t>(telemetry.mode);
  packet[5] = static_cast<uint8_t>(
    (constrain(telemetry.estimatedPodCount, 0, 15) << 4) |
    (telemetry.usbRole & 0x0F)
  );

  const int16_t temperatureCenti = telemetry.climateValid
    ? static_cast<int16_t>(constrain(lroundf(telemetry.temperatureC * 100.0f),
                                     -32768L, 32767L))
    : 0;
  const uint16_t humidityCenti = telemetry.climateValid
    ? static_cast<uint16_t>(constrain(lroundf(telemetry.humidityPercent * 100.0f),
                                      0L, 10000L))
    : 0;
  const uint16_t fanCurrentMilliamps = static_cast<uint16_t>(
    constrain(lroundf(telemetry.fanCurrentAmps * 1000.0f), 0L, 65535L)
  );

  packet[6] = static_cast<uint8_t>(temperatureCenti & 0xFF);
  packet[7] = static_cast<uint8_t>((temperatureCenti >> 8) & 0xFF);
  packet[8] = static_cast<uint8_t>(humidityCenti & 0xFF);
  packet[9] = static_cast<uint8_t>((humidityCenti >> 8) & 0xFF);
  packet[10] = static_cast<uint8_t>(fanCurrentMilliamps & 0xFF);
  packet[11] = static_cast<uint8_t>((fanCurrentMilliamps >> 8) & 0xFF);
  packet[12] = static_cast<uint8_t>(telemetry.batteryMillivolts & 0xFF);
  packet[13] = static_cast<uint8_t>((telemetry.batteryMillivolts >> 8) & 0xFF);
  packet[14] = telemetry.chargerStatus;
  packet[15] = telemetry.chargerFault;

  telemetryCharacteristic->setValue(packet, sizeof(packet));
  if (clientConnected) telemetryCharacteristic->notify();
}
