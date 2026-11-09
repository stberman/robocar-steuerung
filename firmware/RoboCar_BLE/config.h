/*
 * config.h – Hardware-Konstanten und Protokoll-Definitionen
 *
 * Dies ist die EINZIGE Stelle mit Pinbelegung und festen Werten.
 * Gruppennummer, Geschwindigkeit, Trimmung usw. werden NICHT hier,
 * sondern in der App eingestellt und im ESP32 gespeichert.
 */
#pragma once
#include <Arduino.h>

#if !defined(ARDUINO_ARCH_ESP32)
#error "Falsches Board! In der Arduino IDE unter Werkzeuge > Board > esp32 das Board 'ESP32 Dev Module' auswaehlen."
#endif

#define FW_VERSION_MAJOR 1
#define FW_VERSION_MINOR 1

// ---------------------------------------------------------------
// Pinbelegung (siehe Aufbauanleitung WS26/27, Kapitel ESP32)
// ---------------------------------------------------------------
// Motoren A (L298N OUT1/OUT2)
#define PIN_IN1 14
#define PIN_IN2 27
#define PIN_ENA 33   // PWM; NICHT GPIO 12 verwenden (Strapping-Pin -> Bootfehler)

// Motoren B (L298N OUT3/OUT4)
#define PIN_IN3 26
#define PIN_IN4 25
#define PIN_ENB 32   // PWM

// Onboard-LED (blau) des ESP32-DevKit
#define PIN_LED 2

// Frei fuer eigene Erweiterungen (siehe aux_user.cpp):
//   GPIO 4, 16, 17, 18, 19, 21, 22, 23
// Nur Eingang (kein Ausgang moeglich): GPIO 34, 35, 36, 39
// Nicht verwenden: 0, 1, 3, 6-11, 12, 15 (Boot/Flash/USB)

// ---------------------------------------------------------------
// Motor-PWM
// ---------------------------------------------------------------
#define PWM_FREQ_HZ     2000   // L298N schaltet langsam -> niedrige Frequenz
#define PWM_RESOLUTION  8      // 0..255
// Die folgenden Werte sind nur STANDARDWERTE. Sie lassen sich in der App
// unter ⚙ -> Kalibrieren einstellen und werden im Auto gespeichert.
#define DEF_MIN_START_PCT  35  // Anlaufschwelle in % PWM (35 % = PWM 90); darunter drehen die Motoren nicht
#define DEF_CURVE_FULL_PCT 50  // bei diesem Lenkausschlag steht die kurveninnere Seite
#define DEF_RAMP_10MS      13  // Anfahrrampe 0 -> Vollgas in 10-ms-Schritten (13 = 130 ms), 0 = aus
#define DEF_DEADZONE_PCT   5   // Joystick-Ausschlaege darunter werden ignoriert
#define DEF_EXPO_PCT       0   // 0 = linear, 100 = sehr feinfuehlig um die Mitte

// ---------------------------------------------------------------
// Sicherheit
// ---------------------------------------------------------------
#define FAILSAFE_TIMEOUT_MS 300  // so lange ohne Steuerpaket -> Motoren aus

// ---------------------------------------------------------------
// Bluetooth (BLE) – muss mit app/ble.js uebereinstimmen!
// ---------------------------------------------------------------
#define BLE_NAME_PREFIX   "RoboCar-"
#define UUID_SERVICE      "6e8a1000-5c3b-4b6e-9a2f-0c1d2e3f4a50"
#define UUID_CONTROL      "6e8a1001-5c3b-4b6e-9a2f-0c1d2e3f4a50"
#define UUID_CONFIG       "6e8a1002-5c3b-4b6e-9a2f-0c1d2e3f4a50"
#define UUID_STATUS       "6e8a1003-5c3b-4b6e-9a2f-0c1d2e3f4a50"

#define PROTOCOL_VERSION  1

// Steuerpaket (App -> Auto), 9 Byte, ca. 25x pro Sekunde
//   [0] Protokollversion
//   [1] Laufnummer (seq), 0..255
//   [2] Gas      int8  -100..100
//   [3] Lenkung  int8  -100..100 (positiv = rechts)
//   [4] Tasten   Bitmaske (siehe BTN_*)
//   [5..8] Aux 1..4, je 0..255
#define CONTROL_LEN 9
#define BTN_BOOST    0x01
#define BTN_IDENTIFY 0x02
#define BTN_PANZER   0x04   // Panzer-Modus: Gas/Lenkung ohne Lenk-Empfindlichkeit auswerten

// Konfiguration (App <-> Auto), 16 Byte, wird im Flash gespeichert
// (Aeltere Apps senden nur Byte 0..7; die Kalibrierung bleibt dann unveraendert.)
//   [0] Protokollversion
//   [1] Gruppe 0..16 (0 = nicht gesetzt)
//   [2] Max. Geschwindigkeit in %   (10..100)
//   [3] Boost-Geschwindigkeit in %  (10..100)
//   [4] Trimmung int8 -50..50 (positiv = rechte Seite schwaecher)
//   [5] Flags (siehe CFG_*)
//   [6] Lenk-Empfindlichkeit in %   (20..100)
//   [7] reserviert
//   --- Kalibrierung ---
//   [8]  Anlaufschwelle in %            (0..60)
//   [9]  Kurve: innen steht bei Lenkung %   (20..100)
//   [10] Anfahrrampe in 10 ms           (0..50, 0 = aus)
//   [11] Deadzone in %                  (0..20)
//   [12] Expo in %                      (0..100)
//   [13..15] reserviert
#define CONFIG_LEN    16
#define CONFIG_LEN_V1 8
#define CFG_INVERT_A     0x01
#define CFG_INVERT_B     0x02
#define CFG_SWAP_SIDES   0x04
#define CFG_INVERT_STEER 0x08

// Status (Auto -> App, Notify), 6 Byte, bei jedem 5. Steuerpaket
//   [0] FW-Version Major, [1] FW-Version Minor
//   [2] zuletzt empfangene Laufnummer (fuer Latenzmessung)
//   [3] Flags (siehe ST_*)
//   [4] Gruppe
//   [5] reserviert
#define STATUS_LEN 6
#define ST_FAILSAFE  0x01
#define ST_GROUP_SET 0x02
