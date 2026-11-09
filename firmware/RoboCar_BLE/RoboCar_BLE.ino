/*
 * ===================================================================
 *  RoboCar_BLE – Steuerung des Projektwochen-Autos per Handy-App
 *  TH Köln, Projekt Maschinenbau, WS26/27
 * ===================================================================
 *
 *  NAME DES AUTOS (so erscheint es in der Bluetooth-Liste der App)
 *  ---------------------------------------------------------------
 *  Normalerweise heißt das Auto automatisch "RoboCar-07" (07 = eure
 *  Gruppennummer, die ihr in der App einstellt).
 *
 *  Ihr wollt einen eigenen Namen? Dann:
 *    1. Die beiden Schrägstriche // vor "#define AUTO_NAME" unten löschen.
 *    2. Zwischen die Anführungszeichen euren Namen schreiben.
 *    3. Programm hochladen (Pfeil-Taste oben links in der Arduino IDE).
 *
 *  Regeln für den Namen:
 *    - "RoboCar-" wird automatisch davorgesetzt. Aus "Blitz" wird also
 *      "RoboCar-Blitz". Nur Namen mit "RoboCar-" am Anfang zeigt die App an.
 *    - Höchstens 18 Zeichen, keine Umlaute (ä, ö, ü, ß).
 *    - Jedes Auto im Raum braucht einen anderen Namen!
 */
// #define AUTO_NAME "Blitz"

/*
 *  Was brauche ich zum Hochladen?
 *    - Arduino IDE 2.x
 *    - Boardverwalter: "esp32 by Espressif Systems", Version 3.x
 *    - Bibliotheksverwalter: "NimBLE-Arduino" von h2zero, Version 2.x
 *    - Werkzeuge > Board: "ESP32 Dev Module", dazu den richtigen COM-Port
 *
 *  Wie ist das Programm aufgebaut?
 *  Ein Arduino-Programm kann aus mehreren Dateien bestehen. Ihr seht sie
 *  oben in der Arduino IDE als Reiter:
 *    RoboCar_BLE.ino  diese Datei: setup() und loop(), das "Hauptprogramm"
 *    config.h         Pinbelegung und feste Werte
 *    motor.*          Motoren ansteuern
 *    ble_link.*       Bluetooth-Verbindung zum Handy
 *    settings.*       Einstellungen aus der App im Speicher ablegen
 *    aux_user.cpp     >>> HIER baut ihr eure eigenen Erweiterungen ein <<<
 *  Die Dateien mit .h ("Header") sagen nur, WELCHE Funktionen es gibt.
 *  Die Dateien mit .cpp enthalten, WIE die Funktionen arbeiten.
 */

// Gibt es keinen eigenen Namen (Zeile oben auskommentiert), ist der Name leer.
// Dann vergibt das Programm automatisch "RoboCar-<Gruppe>".
#ifndef AUTO_NAME
#define AUTO_NAME ""
#endif

// Die anderen Dateien des Programms einbinden, damit wir ihre Funktionen
// hier benutzen können.
#include "config.h"
#include "settings.h"
#include "motor.h"
#include "ble_link.h"
#include "aux_user.h"

// -------------------------------------------------------------------
// Variablen, die sich das Programm zwischen zwei loop()-Durchläufen merkt.
// "static" heißt: nur in dieser Datei sichtbar.
// -------------------------------------------------------------------
static bool     failsafe = true;              // true = Motoren aus, weil keine Steuerdaten kommen
static uint8_t  lastAux[4] = {0, 0, 0, 0};    // letzte Werte von AUX 1..4 (um Änderungen zu erkennen)
static bool     auxValid = false;             // false = lastAux ist noch nicht gültig
static uint32_t lastTick = 0;                 // Zeitpunkt des letzten loop()-Durchlaufs in ms

// -------------------------------------------------------------------
// Blaue LED auf dem ESP32 als Statusanzeige
//   blinkt langsam  -> wartet auf ein Handy
//   leuchtet        -> verbunden
//   blinkt schnell  -> "Ich bin's!" (Taste "LED blinken lassen" in der App)
// -------------------------------------------------------------------
static void updateLed(bool connected, bool identify) {
  uint32_t t = millis();             // Millisekunden seit dem Einschalten
  bool on;
  // (t / 100) % 2 wechselt alle 100 ms zwischen 0 und 1 -> schnelles Blinken
  if (identify)        on = (t / 100) % 2;
  else if (connected)  on = true;
  else                 on = (t / 500) % 2;   // alle 500 ms -> langsames Blinken
  digitalWrite(PIN_LED, on ? HIGH : LOW);
}

// -------------------------------------------------------------------
// setup() läuft genau EINMAL direkt nach dem Einschalten.
// -------------------------------------------------------------------
void setup() {
  // Serielle Schnittstelle für Meldungen an den PC.
  // Anzeigen: Werkzeuge > Serieller Monitor, 115200 Baud einstellen.
  Serial.begin(115200);
  pinMode(PIN_LED, OUTPUT);

  motorBegin();                        // als Erstes: Motoren sicher ausschalten
  settingsLoad();                      // gespeicherte Einstellungen aus der App laden
  settingsSetCustomName(AUTO_NAME);    // eigenen Namen übernehmen (falls oben gesetzt)
  userSetup();                         // eure eigenen Erweiterungen (aux_user.cpp)
  bleBegin();                          // Bluetooth starten, Auto wird sichtbar

  Serial.printf("RoboCar_BLE v%d.%d gestartet\n", FW_VERSION_MAJOR, FW_VERSION_MINOR);
}

// -------------------------------------------------------------------
// loop() läuft danach immer wieder, endlos.
// -------------------------------------------------------------------
void loop() {
  // Wir arbeiten in einem festen Takt von 10 ms (100x pro Sekunde).
  // Statt delay() zu benutzen (das würde das Programm blockieren), schauen
  // wir nach, ob schon 10 ms vergangen sind. Wenn nicht: sofort zurück.
  uint32_t now = millis();
  if (now - lastTick < 10) return;
  lastTick = now;

  // Was hat die App zuletzt geschickt?
  bool connected = bleIsConnected();
  ControlState c = bleGetControl();    // Gas, Lenkung, Tasten, AUX-Werte

  // --- Failsafe (Sicherheitsabschaltung) ---------------------------
  // Motoren sofort aus, wenn
  //   - kein Handy verbunden ist, oder
  //   - noch nie ein Steuerpaket kam, oder
  //   - das letzte Steuerpaket älter als 300 ms ist (Handy weg, App zu ...)
  bool timeout = !connected || c.receivedAt == 0 || (now - c.receivedAt) > FAILSAFE_TIMEOUT_MS;

  if (timeout) {
    if (!failsafe) {                   // nur beim ersten Mal reagieren, nicht 100x pro Sekunde
      failsafe = true;
      motorStopNow();
      onFailsafe();                    // eure Erweiterungen in sicheren Zustand (aux_user.cpp)
      auxValid = false;
      bleSetFailsafe(true);            // der App melden: "Ich stehe"
      Serial.println("FAILSAFE: keine Steuerdaten -> Motoren aus");
    }
  } else {
    if (failsafe) {                    // Steuerdaten kommen wieder an
      failsafe = false;
      bleSetFailsafe(false);
    }

    // Gas und Lenkung an die Motorsteuerung geben (rechnet in motor.cpp
    // aus, wie schnell die linke und die rechte Seite drehen sollen).
    // "c.buttons & BTN_BOOST" prüft, ob das Bit für die Boost-Taste gesetzt ist.
    motorSetTarget(c.throttle, c.steer, c.buttons & BTN_BOOST, c.buttons & BTN_PANZER);

    // AUX 1..4: Nur wenn sich ein Wert geändert hat, onAux() aufrufen.
    // (Arrays zählen ab 0, die Kanäle heißen aber 1..4, deshalb i + 1.)
    for (uint8_t i = 0; i < 4; i++) {
      if (!auxValid || c.aux[i] != lastAux[i]) {
        lastAux[i] = c.aux[i];
        onAux(i + 1, c.aux[i]);
      }
    }
    auxValid = true;
  }

  motorUpdate();                       // Motoren einen Schritt Richtung Sollwert (Anfahrrampe)
  userLoop();                          // eure eigenen Erweiterungen (aux_user.cpp)
  updateLed(connected, connected && !failsafe && (c.buttons & BTN_IDENTIFY));

  // Wurde in der App eine neue Gruppennummer gespeichert, startet das Auto
  // neu, damit es danach unter dem neuen Namen "RoboCar-XX" erscheint.
  if (bleRestartRequested()) {
    motorStopNow();
    Serial.println("Gruppe geaendert - Neustart");
    delay(300);                        // der App kurz Zeit lassen, die Antwort zu lesen
    ESP.restart();
  }
}
