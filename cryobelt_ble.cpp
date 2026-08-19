#include "cryobelt_ble.h"

#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLESecurity.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <esp_arduino_version.h>

#include "pins.h"

namespace {

constexpr char DEVICE_NAME[] = "CryoBelt";
constexpr char SERVICE_UUID[] = "7d4b1000-6c4a-4f65-9f09-8a2c7d3e1000";
constexpr char COMMAND_UUID[] = "7d4b1001-6c4a-4f65-9f09-8a2c7d3e1000";
constexpr char TELEMETRY_UUID[] = "7d4b1002-6c4a-4f65-9f09-8a2c7d3e1000";

portMUX_TYPE stateMux = portMUX_INITIALIZER_UNLOCKED;
BLECharacteristic* telemetryCharacteristic = nullptr;
constexpr uint8_t COMMAND_QUEUE_SIZE = 8;
uint8_t pendingOpcodes[COMMAND_QUEUE_SIZE] = {};
uint8_t pendingValues[COMMAND_QUEUE_SIZE] = {};
uint8_t commandQueueHead = 0;
uint8_t commandQueueTail = 0;
uint8_t commandQueueCount = 0;
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
    const bool emergencyStop =
      opcode == static_cast<uint8_t>(CryoBeltBLE::Opcode::SET_POWER) &&
      commandValue == 0;
    if (emergencyStop) {
      commandQueueHead = 0;
      commandQueueTail = 0;
      commandQueueCount = 0;
    }
    if (commandQueueCount < COMMAND_QUEUE_SIZE) {
      pendingOpcodes[commandQueueTail] = opcode;
      pendingValues[commandQueueTail] = commandValue;
      commandQueueTail = static_cast<uint8_t>(
        (commandQueueTail + 1) % COMMAND_QUEUE_SIZE
      );
      ++commandQueueCount;
    }
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
    commandQueueHead = 0;
    commandQueueTail = 0;
    commandQueueCount = 0;
    portEXIT_CRITICAL(&stateMux);
    BLEDevice::startAdvertising();
  }
};

class SecurityCallbacks final : public BLESecurityCallbacks {
  uint32_t onPassKeyRequest() override { return 0; }
  void onPassKeyNotify(uint32_t) override {}
  bool onConfirmPIN(uint32_t) override { return false; }

  bool onSecurityRequest() override {
    // Physical-presence gate for first-time bonding. Existing bonds can
    // re-encrypt without repeating this pairing request.
    return digitalRead(PIN_BUTTON_USER) == LOW;
  }

#if ESP_ARDUINO_VERSION_MAJOR >= 3 && defined(CONFIG_NIMBLE_ENABLED)
  void onAuthenticationComplete(ble_gap_conn_desc* result) override {
    if (result == nullptr) {
      Serial.println("[BLE] Pairing failed.");
    }
  }
#else
  void onAuthenticationComplete(esp_ble_auth_cmpl_t result) override {
    if (!result.success) {
      Serial.printf("[BLE] Pairing failed, reason=0x%02X.\n",
                    result.fail_reason);
    }
  }
#endif
};

CommandCallbacks commandCallbacks;
ServerCallbacks serverCallbacks;
SecurityCallbacks securityCallbacks;
BLESecurity security;

} // namespace

bool CryoBeltBLE::begin() {
  BLEDevice::init(DEVICE_NAME);
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  security.setAuthenticationMode(true, false, true);
#else
  BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT_NO_MITM);
  security.setAuthenticationMode(ESP_LE_AUTH_REQ_SC_BOND);
#endif
  BLEDevice::setSecurityCallbacks(&securityCallbacks);
  security.setCapability(ESP_IO_CAP_NONE);
  security.setKeySize(16);
  security.setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
  security.setRespEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);

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

  commandCharacteristic->setAccessPermissions(ESP_GATT_PERM_WRITE_ENCRYPTED);
  telemetryCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED);
  commandCharacteristic->setCallbacks(&commandCallbacks);
  BLE2902* telemetryDescriptor = new BLE2902();
  telemetryDescriptor->setAccessPermissions(
    ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED
  );
  telemetryCharacteristic->addDescriptor(telemetryDescriptor);

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
  const bool available = commandQueueCount != 0;
  if (available) {
    command.opcode = static_cast<Opcode>(pendingOpcodes[commandQueueHead]);
    command.value = pendingValues[commandQueueHead];
    commandQueueHead = static_cast<uint8_t>(
      (commandQueueHead + 1) % COMMAND_QUEUE_SIZE
    );
    --commandQueueCount;
  }
  portEXIT_CRITICAL(&stateMux);
  return available;
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
