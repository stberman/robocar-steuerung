/*
 * ble_link.cpp – BLE-Server mit NimBLE-Arduino (Version 2.x)
 *
 * Ablauf:
 *  - Das Auto sendet Advertising-Pakete unter seinem Namen ("RoboCar-07").
 *  - Ein Handy verbindet sich. Danach stoppt das Advertising automatisch,
 *    andere Handys sehen das Auto nicht mehr (nur eine Verbindung).
 *  - Die App schreibt ca. 25x pro Sekunde ein Steuerpaket.
 *  - Bei jedem 5. Paket schickt das Auto eine Statusmeldung zurueck.
 *  - Nach dem Trennen startet das Advertising wieder.
 */
#include "ble_link.h"
#include "config.h"
#include "settings.h"
#include <NimBLEDevice.h>

static NimBLEServer*         server     = nullptr;
static NimBLECharacteristic* chControl  = nullptr;
static NimBLECharacteristic* chConfig   = nullptr;
static NimBLECharacteristic* chStatus   = nullptr;

static portMUX_TYPE   mux = portMUX_INITIALIZER_UNLOCKED;
static ControlState   control = {};
static volatile bool  connected = false;
static volatile bool  restartRequested = false;
static volatile bool  failsafeActive = true;
static uint8_t        packetCounter = 0;

static void sendStatus(uint8_t seq) {
  uint8_t st[STATUS_LEN];
  st[0] = FW_VERSION_MAJOR;
  st[1] = FW_VERSION_MINOR;
  st[2] = seq;
  st[3] = (failsafeActive ? ST_FAILSAFE : 0) | (settings.group ? ST_GROUP_SET : 0);
  st[4] = settings.group;
  st[5] = 0;
  chStatus->setValue(st, STATUS_LEN);
  chStatus->notify();
}

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* s, NimBLEConnInfo& info) override {
    connected = true;
    // Kurzes Verbindungsintervall anfragen (7,5..15 ms) -> geringe Latenz.
    // Timeout 2 s; das eigentliche Failsafe (300 ms) macht die Firmware selbst.
    s->updateConnParams(info.getConnHandle(), 6, 12, 0, 200);
    Serial.printf("Verbunden mit %s\n", info.getAddress().toString().c_str());
  }
  void onDisconnect(NimBLEServer* s, NimBLEConnInfo& info, int reason) override {
    connected = false;
    portENTER_CRITICAL(&mux);
    control.receivedAt = 0;
    portEXIT_CRITICAL(&mux);
    Serial.printf("Getrennt (Grund %d) - warte auf neue Verbindung\n", reason);
    NimBLEDevice::startAdvertising();
  }
};

class ControlCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c, NimBLEConnInfo& info) override {
    NimBLEAttValue v = c->getValue();
    if (v.size() < CONTROL_LEN || v.data()[0] != PROTOCOL_VERSION) return;
    const uint8_t* d = v.data();
    portENTER_CRITICAL(&mux);
    control.seq        = d[1];
    control.throttle   = (int8_t)constrain((int8_t)d[2], -100, 100);
    control.steer      = (int8_t)constrain((int8_t)d[3], -100, 100);
    control.buttons    = d[4];
    control.aux[0]     = d[5];
    control.aux[1]     = d[6];
    control.aux[2]     = d[7];
    control.aux[3]     = d[8];
    control.receivedAt = millis();
    if (control.receivedAt == 0) control.receivedAt = 1;
    portEXIT_CRITICAL(&mux);

    if (++packetCounter >= 5) {
      packetCounter = 0;
      sendStatus(d[1]);
    }
  }
};

class ConfigCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c, NimBLEConnInfo& info) override {
    NimBLEAttValue v = c->getValue();
    uint8_t oldGroup = settings.group;
    if (settingsFromBytes(v.data(), v.size())) {
      settingsSave();
      Serial.printf("Einstellungen gespeichert: Gruppe %u, Max %u%%, Boost %u%%, Trim %d, Flags 0x%02X, Lenkung %u%%\n",
                    settings.group, settings.maxSpeed, settings.boostSpeed, settings.trim,
                    settings.flags, settings.steerRate);
      Serial.printf("  Kalibrierung: Anlauf %u%%, Kurve %u%%, Rampe %u ms, Deadzone %u%%, Expo %u%%\n",
                    settings.minStart, settings.curveFull, settings.ramp * 10,
                    settings.deadzone, settings.expo);
      if (settings.group != oldGroup) restartRequested = true;
    }
    // Immer den tatsaechlich gespeicherten (ggf. begrenzten) Stand zuruecklesen lassen
    uint8_t buf[CONFIG_LEN];
    settingsToBytes(buf);
    c->setValue(buf, CONFIG_LEN);
  }
};

void bleBegin() {
  String name = deviceName();
  NimBLEDevice::init(name.c_str());
  NimBLEDevice::setPower(9);  // dBm, volle Reichweite im Raum

  server = NimBLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());

  NimBLEService* svc = server->createService(UUID_SERVICE);
  chControl = svc->createCharacteristic(UUID_CONTROL, NIMBLE_PROPERTY::WRITE_NR | NIMBLE_PROPERTY::WRITE);
  chConfig  = svc->createCharacteristic(UUID_CONFIG,  NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
  chStatus  = svc->createCharacteristic(UUID_STATUS,  NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);

  chControl->setCallbacks(new ControlCallbacks());
  chConfig->setCallbacks(new ConfigCallbacks());

  uint8_t buf[CONFIG_LEN];
  settingsToBytes(buf);
  chConfig->setValue(buf, CONFIG_LEN);
  uint8_t st[STATUS_LEN] = {FW_VERSION_MAJOR, FW_VERSION_MINOR, 0, ST_FAILSAFE, settings.group, 0};
  chStatus->setValue(st, STATUS_LEN);

  server->start();

  // Name im Advertising, Service-UUID in der Scan-Antwort (beides zusammen ist zu lang)
  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->setName(name.c_str());
  NimBLEAdvertisementData scanData;
  scanData.setCompleteServices(NimBLEUUID(UUID_SERVICE));
  adv->setScanResponseData(scanData);
  adv->start();

  Serial.printf("Bluetooth bereit als \"%s\"\n", name.c_str());
}

bool bleIsConnected() { return connected; }

ControlState bleGetControl() {
  portENTER_CRITICAL(&mux);
  ControlState c = control;
  portEXIT_CRITICAL(&mux);
  return c;
}

bool bleRestartRequested() { return restartRequested; }

void bleSetFailsafe(bool active) { failsafeActive = active; }
