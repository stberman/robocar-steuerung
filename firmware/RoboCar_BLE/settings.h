/*
 * settings.h – Einstellungen aus der App
 *
 * Alles, was ihr in der App unter ⚙ einstellt, landet hier. Die Werte
 * werden im Flash-Speicher des ESP32 abgelegt und bleiben auch nach dem
 * Ausschalten erhalten.
 */
#pragma once
#include <Arduino.h>

// Ein "struct" fasst mehrere Variablen zu einem Paket zusammen.
// Zugriff z. B. mit settings.maxSpeed
// uint8_t = ganze Zahl 0..255, int8_t = ganze Zahl -128..127
struct Settings {
  uint8_t group;        // Gruppennummer: 0 = nicht gesetzt, sonst 1..16
  uint8_t maxSpeed;     // Max. Geschwindigkeit in %
  uint8_t boostSpeed;   // Geschwindigkeit mit Boost-Taste in %
  int8_t  trim;         // Geradeauslauf -50..50
  uint8_t flags;        // Häkchen "Verkabelung korrigieren" (CFG_* aus config.h)
  uint8_t steerRate;    // Lenk-Empfindlichkeit in %
  // Kalibrierung (ab Firmware 1.1)
  uint8_t minStart;     // Anlaufschwelle in %
  uint8_t curveFull;    // Kurvenschärfe: innere Seite steht bei diesem Lenkausschlag in %
  uint8_t ramp;         // Anfahrrampe in 10-ms-Schritten
  uint8_t deadzone;     // Totbereich in %
  uint8_t expo;         // Feinfühligkeit in %
};

// Die eine, gemeinsame Einstellungs-Variable. "extern" heißt: Sie ist in
// settings.cpp angelegt, aber alle Dateien, die settings.h einbinden,
// dürfen sie benutzen.
extern Settings settings;

// Einstellungen aus dem Flash laden (beim Start) bzw. dort speichern
void settingsLoad();
void settingsSave();

// Einstellungen in ein Datenpaket für die App umwandeln (CONFIG_LEN Byte)
void settingsToBytes(uint8_t* out);

// Ein Datenpaket von der App übernehmen. Zu große oder zu kleine Werte
// werden auf den erlaubten Bereich begrenzt. Gibt false zurück, wenn das
// Paket ungültig ist.
bool settingsFromBytes(const uint8_t* in, size_t len);

// Eigenen Namen aus der .ino übernehmen (AUTO_NAME). Leer = automatischer Name.
void settingsSetCustomName(const char* name);

// Name, unter dem das Auto per Bluetooth sichtbar ist:
//   "RoboCar-" + eigener Name, wenn AUTO_NAME gesetzt ist
//   sonst "RoboCar-07" (Gruppennummer)
//   sonst "RoboCar-A3F1" (aus der Gerätenummer, solange keine Gruppe gesetzt ist)
String deviceName();
