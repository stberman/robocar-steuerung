/*
 * ===================================================================
 *  aux_user.cpp – HIER DÜRFT IHR EURE EIGENEN FUNKTIONEN EINBAUEN
 * ===================================================================
 *
 * Ideen: Kicker (Servo), Licht, Greifarm, Hupe, Sensoren ...
 *
 * Die App hat vier zusätzliche Kanäle AUX 1 bis AUX 4. Jeder Kanal liefert
 * einen Wert von 0 bis 255:
 *   - Taster:   0 = losgelassen, 255 = gedrückt
 *   - Schalter: 0 = aus, 255 = an
 *   - Schieber: 0 .. 255 stufenlos
 * Welcher Kanal Taster, Schalter oder Schieber ist, stellt ihr in der App
 * ein (Datei app/controls.js, ganz oben).
 *
 * Immer wenn sich ein Wert ändert, ruft das Hauptprogramm onAux(kanal, wert)
 * auf. Dort schreibt ihr hinein, was passieren soll.
 *
 * Freie Pins für eure Erweiterungen:  GPIO 4, 16, 17, 18, 19, 21, 22, 23
 * Die Motor-Pins (14, 25, 26, 27, 32, 33) und die LED (2) sind belegt!
 *
 * Achtung Strom: Ein ESP32-Pin liefert nur 3,3 V und höchstens ca. 20 mA.
 * Das reicht für eine LED mit Vorwiderstand, aber NICHT für Servos oder
 * Motoren. Die brauchen eine eigene 5-V-Versorgung (z. B. Powerbank),
 * vom ESP32 kommt nur das Steuersignal. GND (Minus) immer verbinden!
 *
 * Unten stehen zwei fertige Beispiele. Zum Einschalten in der Zeile
 * "#define BEISPIEL_... 0" die 0 durch eine 1 ersetzen und hochladen.
 *
 * Tipp: Im Seriellen Monitor (Werkzeuge > Serieller Monitor, 115200 Baud)
 * seht ihr, welche AUX-Werte ankommen.
 *
 * Wichtig: Nirgends delay() verwenden! Sonst reagiert das Auto während
 * der Wartezeit nicht mehr auf das Handy.
 */
#include "aux_user.h"

// 1 = Beispiel eingeschaltet, 0 = ausgeschaltet
#define BEISPIEL_SERVO_KICKER 0   // Servo an GPIO 4, AUX 1 = Schuss
#define BEISPIEL_LICHT        0   // LED an GPIO 16, AUX 2 = an/aus

// "#if ... #endif": Der Code dazwischen wird nur mitübersetzt, wenn das
// Beispiel oben auf 1 steht.

// -------------------------------------------------------------------
// Beispiel 1: Servo-Kicker an GPIO 4
// Ein Servo bekommt alle 20 ms einen kurzen Puls. Die Länge des Pulses
// bestimmt den Winkel: 1 ms = 0 Grad, 2 ms = 180 Grad.
// -------------------------------------------------------------------
#if BEISPIEL_SERVO_KICKER
const int SERVO_PIN = 4;
const int SERVO_RUHE_GRAD = 20;    // Kicker eingefahren
const int SERVO_SCHUSS_GRAD = 120; // Kicker ausgefahren

// Servo auf einen Winkel (0..180 Grad) stellen
void servoGrad(int grad) {
  grad = constrain(grad, 0, 180);
  int pulsUs = 1000 + grad * 1000 / 180;          // Pulslänge 1000..2000 Mikrosekunden
  int duty = (long)pulsUs * 16383 / 20000;        // in PWM-Wert umrechnen (14 Bit = 0..16383, 20 ms Takt)
  ledcWrite(SERVO_PIN, duty);
}
#endif

// -------------------------------------------------------------------
// Beispiel 2: LED (mit Vorwiderstand!) an GPIO 16
// -------------------------------------------------------------------
#if BEISPIEL_LICHT
const int LICHT_PIN = 16;
#endif

// -------------------------------------------------------------------
// Läuft einmal beim Einschalten: Pins einrichten, Startstellung
// -------------------------------------------------------------------
void userSetup() {
#if BEISPIEL_SERVO_KICKER
  ledcAttach(SERVO_PIN, 50, 14);   // PWM mit 50 Hz (= alle 20 ms), 14 Bit Auflösung
  servoGrad(SERVO_RUHE_GRAD);
#endif
#if BEISPIEL_LICHT
  pinMode(LICHT_PIN, OUTPUT);
#endif
}

// -------------------------------------------------------------------
// Läuft, sobald sich ein AUX-Wert in der App ändert.
//   kanal = 1..4 (welcher AUX-Kanal)
//   wert  = 0..255
// -------------------------------------------------------------------
void onAux(uint8_t kanal, uint8_t wert) {
  // Zum Ausprobieren: Werte im Seriellen Monitor anzeigen
  Serial.printf("AUX %u = %u\n", kanal, wert);

#if BEISPIEL_SERVO_KICKER
  if (kanal == 1) {
    // wert > 127 heißt: Taste gedrückt (mehr als die Hälfte von 255)
    servoGrad(wert > 127 ? SERVO_SCHUSS_GRAD : SERVO_RUHE_GRAD);
  }
#endif

#if BEISPIEL_LICHT
  if (kanal == 2) {
    digitalWrite(LICHT_PIN, wert > 127 ? HIGH : LOW);
  }
#endif
}

// -------------------------------------------------------------------
// Läuft ständig, alle 10 ms. Hier z. B. Sensoren abfragen oder etwas
// zeitgesteuert ablaufen lassen (mit millis() statt delay()).
// -------------------------------------------------------------------
void userLoop() {
}

// -------------------------------------------------------------------
// Läuft, wenn die Verbindung zum Handy abbricht. Hier alles in einen
// sicheren Zustand bringen (Kicker einfahren, Motoren aus ...).
// -------------------------------------------------------------------
void onFailsafe() {
#if BEISPIEL_SERVO_KICKER
  servoGrad(SERVO_RUHE_GRAD);
#endif
#if BEISPIEL_LICHT
  digitalWrite(LICHT_PIN, LOW);
#endif
}
