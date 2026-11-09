/*
 * ble_link.cpp – Bluetooth-Verbindung zur App (Bibliothek NimBLE-Arduino 2.x)
 *
 * Diese Datei müsst ihr NICHT ändern. Sie muss genau zur App passen.
 *
 * So funktioniert Bluetooth Low Energy (BLE) hier:
 *  - Das Auto ist der "Server". Es sendet ständig kurze Funksignale
 *    ("Advertising") mit seinem Namen, z. B. "RoboCar-07". Daran erscheint
 *    es in der Liste der App.
 *  - Das Handy verbindet sich. Danach hört das Advertising auf, andere
 *    Handys sehen das Auto nicht mehr (nur eine Verbindung gleichzeitig).
 *  - Die Daten laufen über drei "Characteristics" (Datenkanäle):
 *      control: Die App schreibt ca. 25x pro Sekunde Gas, Lenkung, Tasten.
 *      config:  Die App liest/schreibt die Einstellungen.
 *      status:  Das Auto meldet bei jedem 5. Paket seinen Zustand zurück.
 *  - Nach dem Trennen startet das Advertising wieder.
 *
 * Wichtig: Die Bluetooth-Bibliothek ruft die "Callbacks" unten selbst auf,
 * sobald Daten ankommen – unabhängig von loop(). Deshalb werden die Daten
 * hier nur abgelegt und loop() holt sie mit bleGetControl() ab.
 */
#include "ble_link.h"
#include "config.h"
#include "settings.h"
#include <NimBLEDevice.h>

// Zeiger auf die Bluetooth-Objekte (werden in bleBegin() angelegt)
static NimBLEServer*         server     = nullptr;
static NimBLECharacteristic* chControl  = nullptr;
static NimBLECharacteristic* chConfig   = nullptr;
static NimBLECharacteristic* chStatus   = nullptr;

// "mux" sorgt dafür, dass loop() nie ein halb geschriebenes Paket liest
// (Bluetooth und loop() laufen auf dem ESP32 gleichzeitig).
static portMUX_TYPE   mux = portMUX_INITIALIZER_UNLOCKED;
static ControlState   control = {};
// "volatile": Die Variable wird von einem anderen Programmteil geändert,
// der Compiler soll sie deshalb jedes Mal neu lesen.
static volatile bool  connected = false;
static volatile bool  restartRequested = false;
static volatile bool  failsafeActive = true;
static uint8_t        packetCounter = 0;

// Statusmeldung an die App schicken (Aufbau siehe "Status" in config.h)
static void sendStatus(uint8_t seq) {
  uint8_t st[STATUS_LEN];
  st[0] = FW_VERSION_MAJOR;
  st[1] = FW_VERSION_MINOR;
  st[2] = seq;                 // Laufnummer zurückschicken -> App misst daraus die Verzögerung
  st[3] = (failsafeActive ? ST_FAILSAFE : 0) | (settings.group ? ST_GROUP_SET : 0);
  st[4] = settings.group;
  st[5] = 0;
  chStatus->setValue(st, STATUS_LEN);
  chStatus->notify();          // an die App senden
}

// -------------------------------------------------------------------
// Wird aufgerufen, wenn sich ein Handy verbindet oder trennt
// -------------------------------------------------------------------
class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* s, NimBLEConnInfo& info) override {
    connected = true;
    // Kurzen Funk-Takt anfragen (alle 7,5..15 ms) -> Auto reagiert schnell.
    // Die Bluetooth-Verbindung gilt erst nach 2 s als verloren; das eigentliche
    // Failsafe (300 ms) macht das Programm in loop() selbst.
    s->updateConnParams(info.getConnHandle(), 6, 12, 0, 200);
    Serial.printf("Verbunden mit %s\n", info.getAddress().toString().c_str());
  }
  void onDisconnect(NimBLEServer* s, NimBLEConnInfo& info, int reason) override {
    connected = false;
    portENTER_CRITICAL(&mux);
    control.receivedAt = 0;    // alte Steuerdaten ungültig machen
    portEXIT_CRITICAL(&mux);
    Serial.printf("Getrennt (Grund %d) - warte auf neue Verbindung\n", reason);
    NimBLEDevice::startAdvertising();   // wieder sichtbar werden
  }
};

// -------------------------------------------------------------------
// Wird aufgerufen, wenn die App ein Steuerpaket schickt (ca. 25x pro Sekunde)
// -------------------------------------------------------------------
class ControlCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c, NimBLEConnInfo& info) override {
    NimBLEAttValue v = c->getValue();
    // Zu kurz oder falsche Protokollversion -> Paket ignorieren
    if (v.size() < CONTROL_LEN || v.data()[0] != PROTOCOL_VERSION) return;
    const uint8_t* d = v.data();

    // Werte übernehmen (Aufbau siehe "Steuerpaket" in config.h)
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
    if (control.receivedAt == 0) control.receivedAt = 1;   // 0 bedeutet "noch nie empfangen"
    portEXIT_CRITICAL(&mux);

    // Bei jedem 5. Paket eine Statusmeldung zurückschicken
    if (++packetCounter >= 5) {
      packetCounter = 0;
      sendStatus(d[1]);
    }
  }
};

// -------------------------------------------------------------------
// Wird aufgerufen, wenn die App Einstellungen schreibt
// -------------------------------------------------------------------
class ConfigCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c, NimBLEConnInfo& info) override {
    NimBLEAttValue v = c->getValue();
    uint8_t oldGroup = settings.group;
    if (settingsFromBytes(v.data(), v.size())) {
      settingsSave();          // dauerhaft im Flash speichern
      Serial.printf("Einstellungen gespeichert: Gruppe %u, Max %u%%, Boost %u%%, Trim %d, Flags 0x%02X, Lenkung %u%%\n",
                    settings.group, settings.maxSpeed, settings.boostSpeed, settings.trim,
                    settings.flags, settings.steerRate);
      Serial.printf("  Kalibrierung: Anlauf %u%%, Kurve %u%%, Rampe %u ms, Deadzone %u%%, Expo %u%%\n",
                    settings.minStart, settings.curveFull, settings.ramp * 10,
                    settings.deadzone, settings.expo);
      // Neue Gruppe -> neuer Name -> Neustart nötig (passiert in loop())
      if (settings.group != oldGroup) restartRequested = true;
    }
    // Der App immer den tatsächlich gespeicherten Stand zum Zurücklesen
    // bereitstellen (Werte wurden evtl. begrenzt)
    uint8_t buf[CONFIG_LEN];
    settingsToBytes(buf);
    c->setValue(buf, CONFIG_LEN);
  }
};

// -------------------------------------------------------------------
// Bluetooth einrichten und starten
// -------------------------------------------------------------------
void bleBegin() {
  String name = deviceName();
  NimBLEDevice::init(name.c_str());
  NimBLEDevice::setPower(9);  // Sendeleistung in dBm (volle Reichweite im Raum)

  server = NimBLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());

  // Dienst (Service) mit den drei Datenkanälen anlegen
  NimBLEService* svc = server->createService(UUID_SERVICE);
  chControl = svc->createCharacteristic(UUID_CONTROL, NIMBLE_PROPERTY::WRITE_NR | NIMBLE_PROPERTY::WRITE);
  chConfig  = svc->createCharacteristic(UUID_CONFIG,  NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
  chStatus  = svc->createCharacteristic(UUID_STATUS,  NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);

  chControl->setCallbacks(new ControlCallbacks());
  chConfig->setCallbacks(new ConfigCallbacks());

  // Startwerte, die die App lesen kann
  uint8_t buf[CONFIG_LEN];
  settingsToBytes(buf);
  chConfig->setValue(buf, CONFIG_LEN);
  uint8_t st[STATUS_LEN] = {FW_VERSION_MAJOR, FW_VERSION_MINOR, 0, ST_FAILSAFE, settings.group, 0};
  chStatus->setValue(st, STATUS_LEN);

  server->start();

  // Advertising: Der Name kommt ins erste Funkpaket, die Dienst-Kennung (UUID)
  // in die "Scan-Antwort". Beides zusammen passt nicht in ein Paket.
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
  // Kopie unter Schutz anlegen, damit sich das Paket währenddessen nicht ändert
  portENTER_CRITICAL(&mux);
  ControlState c = control;
  portEXIT_CRITICAL(&mux);
  return c;
}

bool bleRestartRequested() { return restartRequested; }

void bleSetFailsafe(bool active) { failsafeActive = active; }
