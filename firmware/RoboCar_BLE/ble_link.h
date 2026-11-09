/*
 * ble_link.h – Bluetooth-Verbindung zur App
 *
 * Diese Datei müsst ihr NICHT ändern. Sie muss genau zur App passen.
 */
#pragma once
#include <Arduino.h>

// Das zuletzt von der App empfangene Steuerpaket
struct ControlState {
  uint8_t  seq;          // Laufnummer des Pakets (zählt 0..255 hoch)
  int8_t   throttle;     // Gas -100..100
  int8_t   steer;        // Lenkung -100..100
  uint8_t  buttons;      // Tasten als Bits (BTN_* in config.h)
  uint8_t  aux[4];       // AUX 1..4, je 0..255
  uint32_t receivedAt;   // Zeitpunkt des Empfangs in ms (millis()), 0 = noch nie
};

// Bluetooth starten: Das Auto wird unter seinem Namen sichtbar.
void bleBegin();

// Ist gerade ein Handy verbunden?
bool bleIsConnected();

// Kopie des letzten Steuerpakets abholen
ControlState bleGetControl();

// true, wenn die App eine neue Gruppennummer gesetzt hat. Das Auto muss dann
// neu starten, damit es unter dem neuen Namen gefunden wird.
bool bleRestartRequested();

// Der App melden, ob die Motoren wegen Failsafe stehen
void bleSetFailsafe(bool active);
