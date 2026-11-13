/*
 * aux_user.cpp – Variante "Kicker mit Automatik"
 *
 * Ein Tipp auf KICK (AUX 1) löst genau EINEN Schuss aus:
 * Der Servo fährt aus und nach SCHUSS_DAUER_MS automatisch wieder ein,
 * auch wenn die Taste länger gedrückt bleibt.
 *
 * Verdrahtung: Servo-Signal (orange) an GPIO 4,
 *              Servo-Plus (rot) an VIN, Servo-Minus (braun) an GND.
 */
#include "aux_user.h"

const int SERVO_PIN = 4;
const int SERVO_RUHE_GRAD = 20;            // Kicker eingefahren
const int SERVO_SCHUSS_GRAD = 120;         // Kicker ausgefahren
const unsigned long SCHUSS_DAUER_MS = 250; // so lange bleibt der Kicker draußen

// Diese Variablen merkt sich das Programm zwischen den Aufrufen
bool schussLaeuft = false;        // true = Kicker ist gerade draußen
unsigned long schussStart = 0;    // Zeitpunkt (millis) des Schusses
uint8_t letzterWert = 0;          // letzter Wert von AUX 1

// Servo auf einen Winkel (0..180 Grad) stellen
void servoGrad(int grad) {
  grad = constrain(grad, 0, 180);
  int pulsUs = 1000 + grad * 1000 / 180;     // Pulslänge 1000..2000 Mikrosekunden
  int duty = (long)pulsUs * 16383 / 20000;   // in PWM-Wert umrechnen (14 Bit, 20 ms Takt)
  ledcWrite(SERVO_PIN, duty);
}

void userSetup() {
  ledcAttach(SERVO_PIN, 50, 14);   // PWM mit 50 Hz (= alle 20 ms), 14 Bit Auflösung
  servoGrad(SERVO_RUHE_GRAD);
}

void onAux(uint8_t kanal, uint8_t wert) {
  Serial.printf("AUX %u = %u\n", kanal, wert);

  if (kanal == 1) {
    // Nur beim DRÜCKEN schießen (Wert war klein, ist jetzt groß),
    // und nur, wenn nicht schon ein Schuss läuft.
    bool gedrueckt = (wert > 127) && (letzterWert <= 127);
    if (gedrueckt && !schussLaeuft) {
      servoGrad(SERVO_SCHUSS_GRAD);
      schussLaeuft = true;
      schussStart = millis();          // Uhrzeit merken
    }
    letzterWert = wert;
  }
}

void userLoop() {
  // Ist die Schusszeit abgelaufen? Dann Kicker wieder einfahren.
  if (schussLaeuft && millis() - schussStart >= SCHUSS_DAUER_MS) {
    servoGrad(SERVO_RUHE_GRAD);
    schussLaeuft = false;
  }
}

void onFailsafe() {
  servoGrad(SERVO_RUHE_GRAD);   // Verbindung weg: Kicker einfahren
  schussLaeuft = false;
  letzterWert = 0;
}
