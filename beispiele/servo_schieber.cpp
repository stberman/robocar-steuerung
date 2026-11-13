/*
 * aux_user.cpp – Variante "Servo per Schieber"
 *
 * Der Schieber ARM (AUX 3) stellt den Servo-Winkel stufenlos ein,
 * z. B. für einen Greifarm oder eine Klappe.
 *
 * Verdrahtung: Servo-Signal (orange) an GPIO 4,
 *              Servo-Plus (rot) an VIN, Servo-Minus (braun) an GND.
 */
#include "aux_user.h"

const int SERVO_PIN = 4;
const int WINKEL_MIN = 0;     // Winkel bei Schieber ganz links
const int WINKEL_MAX = 180;   // Winkel bei Schieber ganz rechts
const int WINKEL_START = 90;  // Winkel nach dem Einschalten und bei Failsafe

// Servo auf einen Winkel (0..180 Grad) stellen
void servoGrad(int grad) {
  grad = constrain(grad, 0, 180);
  int pulsUs = 1000 + grad * 1000 / 180;     // Pulslänge 1000..2000 Mikrosekunden
  int duty = (long)pulsUs * 16383 / 20000;   // in PWM-Wert umrechnen (14 Bit, 20 ms Takt)
  ledcWrite(SERVO_PIN, duty);
}

void userSetup() {
  ledcAttach(SERVO_PIN, 50, 14);
  servoGrad(WINKEL_START);
}

void onAux(uint8_t kanal, uint8_t wert) {
  Serial.printf("AUX %u = %u\n", kanal, wert);

  if (kanal == 3) {
    // map() rechnet einen Bereich in einen anderen um:
    // Schieberwert 0..255  ->  Winkel WINKEL_MIN..WINKEL_MAX
    int grad = map(wert, 0, 255, WINKEL_MIN, WINKEL_MAX);
    servoGrad(grad);
  }
}

void userLoop() {
}

void onFailsafe() {
  servoGrad(WINKEL_START);
}
