/*
 * ===================================================================
 *  aux_user.cpp – HIER DUERFT IHR EURE EIGENEN FUNKTIONEN EINBAUEN
 * ===================================================================
 *
 * Die App hat vier Aux-Kanaele (AUX 1 bis AUX 4). Jeder Kanal liefert einen
 * Wert von 0 bis 255:
 *   - Taster:   0 = losgelassen, 255 = gedrueckt
 *   - Schalter: 0 = aus, 255 = an
 *   - Schieber: 0 .. 255 stufenlos
 * Welcher Kanal welche Art hat, stellt ihr in der App ein (app/controls.js).
 *
 * Immer wenn sich ein Wert aendert, wird onAux(kanal, wert) aufgerufen.
 *
 * Freie Pins fuer eure Erweiterungen:  GPIO 4, 16, 17, 18, 19, 21, 22, 23
 * Die Motor-Pins (14, 25, 26, 27, 32, 33) und die LED (2) sind belegt!
 *
 * Achtung: Ein ESP32-Pin liefert nur 3,3 V und max. ca. 20 mA.
 * Servos und Motoren brauchen eine eigene 5-V-Versorgung (z. B. Powerbank),
 * nur das Signal kommt vom ESP32. GND immer verbinden!
 *
 * Unten stehen zwei Beispiele. Zum Aktivieren die 0 durch eine 1 ersetzen.
 */
#include "aux_user.h"

#define BEISPIEL_SERVO_KICKER 0   // Servo an GPIO 4, AUX 1 = Schuss
#define BEISPIEL_LICHT        0   // LED an GPIO 16, AUX 2 = an/aus

// -------------------------------------------------------------------
// Beispiel 1: Servo-Kicker an GPIO 4
// Ein Servo erwartet alle 20 ms einen Puls von 1 ms (0 Grad) bis 2 ms (180 Grad).
// -------------------------------------------------------------------
#if BEISPIEL_SERVO_KICKER
const int SERVO_PIN = 4;
const int SERVO_RUHE_GRAD = 20;    // Kicker eingefahren
const int SERVO_SCHUSS_GRAD = 120; // Kicker ausgefahren

void servoGrad(int grad) {
  grad = constrain(grad, 0, 180);
  int pulsUs = 1000 + grad * 1000 / 180;          // 1000..2000 us
  int duty = (long)pulsUs * 16383 / 20000;        // 14 Bit bei 50 Hz
  ledcWrite(SERVO_PIN, duty);
}
#endif

#if BEISPIEL_LICHT
const int LICHT_PIN = 16;
#endif

void userSetup() {
#if BEISPIEL_SERVO_KICKER
  ledcAttach(SERVO_PIN, 50, 14);   // 50 Hz, 14 Bit Aufloesung
  servoGrad(SERVO_RUHE_GRAD);
#endif
#if BEISPIEL_LICHT
  pinMode(LICHT_PIN, OUTPUT);
#endif
}

void onAux(uint8_t kanal, uint8_t wert) {
  // Zum Ausprobieren: Werte im Seriellen Monitor (115200 Baud) anzeigen
  Serial.printf("AUX %u = %u\n", kanal, wert);

#if BEISPIEL_SERVO_KICKER
  if (kanal == 1) {
    servoGrad(wert > 127 ? SERVO_SCHUSS_GRAD : SERVO_RUHE_GRAD);
  }
#endif

#if BEISPIEL_LICHT
  if (kanal == 2) {
    digitalWrite(LICHT_PIN, wert > 127 ? HIGH : LOW);
  }
#endif
}

void userLoop() {
  // Wird alle 10 ms aufgerufen. Hier z. B. Sensoren abfragen.
  // Kein delay() verwenden, sonst reagiert das Auto nicht mehr!
}

void onFailsafe() {
  // Verbindung verloren: alles in einen sicheren Zustand bringen.
#if BEISPIEL_SERVO_KICKER
  servoGrad(SERVO_RUHE_GRAD);
#endif
#if BEISPIEL_LICHT
  digitalWrite(LICHT_PIN, LOW);
#endif
}
