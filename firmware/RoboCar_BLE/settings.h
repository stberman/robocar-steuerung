/*
 * settings.h – Einstellungen, die die App setzt und die im Flash (NVS)
 * gespeichert werden. Sie bleiben auch nach dem Ausschalten erhalten.
 */
#pragma once
#include <Arduino.h>

struct Settings {
  uint8_t group;        // 0 = nicht gesetzt, sonst 1..16
  uint8_t maxSpeed;     // %
  uint8_t boostSpeed;   // %
  int8_t  trim;         // -50..50
  uint8_t flags;        // CFG_* aus config.h
  uint8_t steerRate;    // %
};

extern Settings settings;

void settingsLoad();
void settingsSave();

// Wandelt zwischen Settings und dem 8-Byte-Konfigurationspaket um.
void settingsToBytes(uint8_t* out);
// Uebernimmt gueltige Werte aus dem Paket (Werte werden begrenzt).
// Gibt false zurueck, wenn das Paket ungueltig ist.
bool settingsFromBytes(const uint8_t* in, size_t len);

// "RoboCar-07" bzw. "RoboCar-A3F1" (aus der MAC), wenn keine Gruppe gesetzt ist
String deviceName();
