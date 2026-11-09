/*
 * RoboCar_BLE – Steuerung des Projektwochen-Autos per Handy-App (Bluetooth LE)
 * TH Koeln, Projekt Maschinenbau, WS26/27
 *
 * Benoetigt:
 *   - Arduino IDE 2.x mit "esp32 by Espressif Systems" Version 3.x
 *   - Bibliothek "NimBLE-Arduino" von h2zero, Version 2.x (Bibliotheksverwalter)
 *   - Board: "ESP32 Dev Module"
 *
 * Alle Autos bekommen dieselbe Firmware. Gruppennummer, Geschwindigkeit,
 * Trimmung und Motorrichtung werden in der App eingestellt.
 *
 * Dateien:
 *   config.h      Pinbelegung, Protokoll
 *   motor.*       Motoransteuerung (Differential-Antrieb, Rampe)
 *   ble_link.*    Bluetooth-Verbindung
 *   settings.*    gespeicherte Einstellungen
 *   aux_user.cpp  EIGENE ERWEITERUNGEN (fuer die Studierenden)
 */
#include "config.h"
#include "settings.h"
#include "motor.h"
#include "ble_link.h"
#include "aux_user.h"

static bool     failsafe = true;
static uint8_t  lastAux[4] = {0, 0, 0, 0};
static bool     auxValid = false;
static uint32_t lastTick = 0;

static void updateLed(bool connected, bool identify) {
  uint32_t t = millis();
  bool on;
  if (identify)        on = (t / 100) % 2;   // schnell blinken: "Ich bin's!"
  else if (connected)  on = true;            // verbunden: Dauerlicht
  else                 on = (t / 500) % 2;   // wartet auf Verbindung: langsam blinken
  digitalWrite(PIN_LED, on ? HIGH : LOW);
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_LED, OUTPUT);

  motorBegin();          // zuerst: Motoren sicher aus
  settingsLoad();
  userSetup();
  bleBegin();

  Serial.printf("RoboCar_BLE v%d.%d gestartet\n", FW_VERSION_MAJOR, FW_VERSION_MINOR);
}

void loop() {
  // Fester 10-ms-Takt ohne delay()
  uint32_t now = millis();
  if (now - lastTick < 10) return;
  lastTick = now;

  bool connected = bleIsConnected();
  ControlState c = bleGetControl();

  // --- Failsafe: keine Verbindung oder zu lange kein Steuerpaket ---
  bool timeout = !connected || c.receivedAt == 0 || (now - c.receivedAt) > FAILSAFE_TIMEOUT_MS;
  if (timeout) {
    if (!failsafe) {
      failsafe = true;
      motorStopNow();
      onFailsafe();
      auxValid = false;
      bleSetFailsafe(true);
      Serial.println("FAILSAFE: keine Steuerdaten -> Motoren aus");
    }
  } else {
    if (failsafe) {
      failsafe = false;
      bleSetFailsafe(false);
    }
    motorSetTarget(c.throttle, c.steer, c.buttons & BTN_BOOST);

    for (uint8_t i = 0; i < 4; i++) {
      if (!auxValid || c.aux[i] != lastAux[i]) {
        lastAux[i] = c.aux[i];
        onAux(i + 1, c.aux[i]);
      }
    }
    auxValid = true;
  }

  motorUpdate();
  userLoop();
  updateLed(connected, connected && !failsafe && (c.buttons & BTN_IDENTIFY));

  // Neue Gruppennummer -> Neustart, damit das Auto unter dem neuen Namen erscheint
  if (bleRestartRequested()) {
    motorStopNow();
    Serial.println("Gruppe geaendert - Neustart");
    delay(300);   // App kann die Antwort noch lesen
    ESP.restart();
  }
}
