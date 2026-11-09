/*
 * motor.h – Ansteuerung der beiden Motorpaare über den L298N
 *
 * Hier steht nur, WELCHE Funktionen es gibt. Wie sie arbeiten,
 * steht in motor.cpp.
 */
#pragma once
#include <Arduino.h>

// Motor-Pins einrichten und Motoren ausschalten. Einmal in setup() aufrufen.
void motorBegin();

// Neue Sollwerte aus der App übernehmen.
//   throttle = Gas      -100..100 (negativ = rückwärts)
//   steer    = Lenkung  -100..100 (positiv = rechts)
//   boost    = Boost-Taste gedrückt?
//   panzer   = Panzer-Modus in der App?
// Die Motoren fahren danach in motorUpdate() schrittweise auf diese Werte.
void motorSetTarget(int8_t throttle, int8_t steer, bool boost, bool panzer);

// Alle 10 ms aufrufen: bringt die Motoren einen Schritt näher an den Sollwert.
void motorUpdate();

// Sofort anhalten, ohne Rampe (Failsafe, Verbindungsabbruch).
void motorStopNow();
