/*
 * aux_user.h – Schnittstelle fuer eigene Erweiterungen (siehe aux_user.cpp)
 */
#pragma once
#include <Arduino.h>

void userSetup();                           // einmal beim Start
void onAux(uint8_t kanal, uint8_t wert);     // Aux-Kanal 1..4 hat sich geaendert, Wert 0..255
void userLoop();                            // staendig (alle 10 ms)
void onFailsafe();                          // Verbindung weg -> eigene Aktoren in sicheren Zustand
