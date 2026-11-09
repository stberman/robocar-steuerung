/*
 * ble_link.h – Bluetooth-Low-Energy-Verbindung zur App
 */
#pragma once
#include <Arduino.h>

// Letztes empfangenes Steuerpaket (Kopie, threadsicher abgeholt)
struct ControlState {
  uint8_t  seq;
  int8_t   throttle;
  int8_t   steer;
  uint8_t  buttons;
  uint8_t  aux[4];
  uint32_t receivedAt;   // millis() beim Empfang, 0 = noch nie
};

void bleBegin();
bool bleIsConnected();
ControlState bleGetControl();

// true, wenn die App eine neue Gruppennummer gesetzt hat. Das Auto muss dann
// neu starten, damit es unter dem neuen Namen gefunden wird.
bool bleRestartRequested();

// Failsafe-Zustand fuer die Statusmeldung an die App
void bleSetFailsafe(bool active);
