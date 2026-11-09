/*
 * motor.h – Ansteuerung der beiden Motorpaare ueber den L298N
 */
#pragma once
#include <Arduino.h>

void motorBegin();

// Sollwerte setzen: throttle und steer jeweils -100..100, boost = Boost-Taste gedrueckt.
// Die Motoren laufen mit Rampe in motorUpdate() auf diese Werte.
void motorSetTarget(int8_t throttle, int8_t steer, bool boost);

// Alle 10 ms aufrufen (Rampe).
void motorUpdate();

// Sofort stoppen, ohne Rampe (Failsafe, Verbindungsabbruch).
void motorStopNow();
