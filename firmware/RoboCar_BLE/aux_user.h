/*
 * aux_user.h – Funktionen für eure eigenen Erweiterungen
 *
 * Was die Funktionen tun, programmiert ihr in aux_user.cpp.
 * Aufgerufen werden sie automatisch vom Hauptprogramm (RoboCar_BLE.ino).
 */
#pragma once
#include <Arduino.h>

void userSetup();                           // einmal beim Start
void onAux(uint8_t kanal, uint8_t wert);    // AUX-Kanal 1..4 hat sich geändert, Wert 0..255
void userLoop();                            // ständig, alle 10 ms
void onFailsafe();                          // Verbindung weg -> eigene Aktoren in sicheren Zustand
