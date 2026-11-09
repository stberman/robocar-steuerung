/*
 * config.h – Pinbelegung und feste Werte
 *
 * Dies ist die EINZIGE Stelle mit Pinnummern und festen Zahlenwerten.
 * Gruppennummer, Geschwindigkeit, Trimmung usw. stehen NICHT hier,
 * sondern werden in der App eingestellt und im ESP32 gespeichert.
 *
 * Was ist "#define"?
 *   #define PIN_IN1 14  bedeutet: Überall, wo im Programm PIN_IN1 steht,
 *   setzt der Compiler die Zahl 14 ein. So hat jede Zahl einen
 *   verständlichen Namen, und man muss sie nur an einer Stelle ändern.
 */
#pragma once          // sorgt dafür, dass diese Datei nur einmal eingebunden wird
#include <Arduino.h>

// Schutz vor dem häufigsten Fehler: falsches Board in der Arduino IDE gewählt.
// Dann bricht das Kompilieren mit dieser verständlichen Meldung ab.
#if !defined(ARDUINO_ARCH_ESP32)
#error "Falsches Board! In der Arduino IDE unter Werkzeuge > Board > esp32 das Board 'ESP32 Dev Module' auswaehlen."
#endif

// Versionsnummer dieses Programms (zeigt die App an)
#define FW_VERSION_MAJOR 1
#define FW_VERSION_MINOR 1

// ---------------------------------------------------------------
// Pinbelegung: welcher ESP32-Pin ist mit welchem L298N-Anschluss verbunden
// (siehe Aufbauanleitung WS26/27, Kapitel 5.2)
// ---------------------------------------------------------------
// Motoren A (L298N-Ausgänge OUT1/OUT2)
#define PIN_IN1 14    // Drehrichtung
#define PIN_IN2 27    // Drehrichtung
#define PIN_ENA 33    // Geschwindigkeit (PWM). NICHT GPIO 12 verwenden: Strapping-Pin -> ESP32 startet evtl. nicht

// Motoren B (L298N-Ausgänge OUT3/OUT4)
#define PIN_IN3 26    // Drehrichtung
#define PIN_IN4 25    // Drehrichtung
#define PIN_ENB 32    // Geschwindigkeit (PWM)

// Blaue LED, die schon auf dem ESP32-Board verbaut ist
#define PIN_LED 2

// Frei für eigene Erweiterungen (siehe aux_user.cpp):
//   GPIO 4, 16, 17, 18, 19, 21, 22, 23
// Nur als Eingang nutzbar (z. B. für Sensoren): GPIO 34, 35, 36, 39
// Nicht verwenden: 0, 1, 3, 6-11, 12, 15 (werden zum Starten, Flashen und für USB gebraucht)

// ---------------------------------------------------------------
// Motor-PWM
// PWM = Pulsweitenmodulation: Der Pin wird sehr schnell ein- und
// ausgeschaltet. Je länger er pro Takt eingeschaltet ist, desto
// schneller dreht der Motor. Wert 0 = aus, 255 = volle Kraft.
// ---------------------------------------------------------------
#define PWM_FREQ_HZ     2000   // 2000 Takte pro Sekunde (der L298N schaltet langsam, daher niedrig)
#define PWM_RESOLUTION  8      // 8 Bit -> Werte von 0 bis 255

// Die folgenden Werte sind nur STANDARDWERTE für ein neues Auto.
// Ihr stellt sie in der App unter ⚙ -> Kalibrieren ein, dort werden sie
// im Auto gespeichert. Hier also bitte nichts ändern.
#define DEF_MIN_START_PCT  35  // Anlaufschwelle in % (35 % = PWM 90). Darunter drehen die Motoren nicht
#define DEF_CURVE_FULL_PCT 50  // bei diesem Lenkausschlag bleibt die kurveninnere Seite stehen
#define DEF_RAMP_10MS      13  // Anfahrrampe von 0 auf Vollgas in 10-ms-Schritten (13 = 130 ms), 0 = aus
#define DEF_DEADZONE_PCT   5   // Joystick-Ausschläge darunter werden ignoriert
#define DEF_EXPO_PCT       0   // 0 = linear, 100 = sehr feinfühlig um die Mitte

// ---------------------------------------------------------------
// Sicherheit
// ---------------------------------------------------------------
#define FAILSAFE_TIMEOUT_MS 300  // kommt so lange (in ms) kein Steuerpaket -> Motoren aus

// ---------------------------------------------------------------
// Bluetooth (BLE = Bluetooth Low Energy)
// Diese Werte müssen genau zur App passen (app/ble.js)! Nicht ändern.
//
// Eine UUID ist eine weltweit eindeutige Kennung. Daran erkennt die App
// "ihren" Dienst auf dem Auto und die drei Datenkanäle (Characteristics):
//   control = Steuerdaten  App -> Auto
//   config  = Einstellungen App <-> Auto
//   status  = Rückmeldung  Auto -> App
// ---------------------------------------------------------------
#define BLE_NAME_PREFIX   "RoboCar-"   // jeder Autoname beginnt damit
#define UUID_SERVICE      "6e8a1000-5c3b-4b6e-9a2f-0c1d2e3f4a50"
#define UUID_CONTROL      "6e8a1001-5c3b-4b6e-9a2f-0c1d2e3f4a50"
#define UUID_CONFIG       "6e8a1002-5c3b-4b6e-9a2f-0c1d2e3f4a50"
#define UUID_STATUS       "6e8a1003-5c3b-4b6e-9a2f-0c1d2e3f4a50"

#define PROTOCOL_VERSION  1

// Aufbau der Datenpakete. Ein Paket ist eine Reihe von Bytes
// (1 Byte = Zahl von 0 bis 255). [2] heißt: das Byte an Stelle 2 (gezählt ab 0).
//
// Steuerpaket (App -> Auto), 9 Byte, ca. 25x pro Sekunde
//   [0] Protokollversion
//   [1] Laufnummer (seq), 0..255
//   [2] Gas      -100..100 (negativ = rückwärts)
//   [3] Lenkung  -100..100 (positiv = rechts)
//   [4] Tasten   ein Bit pro Taste (siehe BTN_*)
//   [5..8] AUX 1..4, je 0..255
#define CONTROL_LEN 9
#define BTN_BOOST    0x01   // Bit 0: Boost-Taste gedrückt
#define BTN_IDENTIFY 0x02   // Bit 1: "LED blinken lassen"
#define BTN_PANZER   0x04   // Bit 2: Panzer-Modus (Gas/Lenkung ohne Lenk-Empfindlichkeit auswerten)

// Konfiguration (App <-> Auto), 16 Byte, wird im Speicher des ESP32 abgelegt
// (Ältere Apps senden nur Byte 0..7; die Kalibrierung bleibt dann unverändert.)
//   [0] Protokollversion
//   [1] Gruppe 0..16 (0 = nicht gesetzt)
//   [2] Max. Geschwindigkeit in %   (10..100)
//   [3] Boost-Geschwindigkeit in %  (10..100)
//   [4] Trimmung -50..50 (positiv = rechte Seite schwächer)
//   [5] Flags (siehe CFG_*)
//   [6] Lenk-Empfindlichkeit in %   (20..100)
//   [7] reserviert
//   --- Kalibrierung ---
//   [8]  Anlaufschwelle in %                (0..60)
//   [9]  Kurve: innen steht bei Lenkung %   (20..100)
//   [10] Anfahrrampe in 10 ms               (0..50, 0 = aus)
//   [11] Totbereich (Deadzone) in %         (0..20)
//   [12] Expo in %                          (0..100)
//   [13..15] reserviert
#define CONFIG_LEN    16
#define CONFIG_LEN_V1 8
#define CFG_INVERT_A     0x01   // Motoren A drehen falsch herum
#define CFG_INVERT_B     0x02   // Motoren B drehen falsch herum
#define CFG_SWAP_SIDES   0x04   // links und rechts vertauscht
#define CFG_INVERT_STEER 0x08   // Lenkung umkehren

// Status (Auto -> App), 6 Byte, bei jedem 5. Steuerpaket
//   [0] Versionsnummer Major, [1] Versionsnummer Minor
//   [2] zuletzt empfangene Laufnummer (daraus misst die App die Verzögerung)
//   [3] Flags (siehe ST_*)
//   [4] Gruppe
//   [5] reserviert
#define STATUS_LEN 6
#define ST_FAILSAFE  0x01   // Motoren wegen fehlender Steuerdaten aus
#define ST_GROUP_SET 0x02   // Gruppennummer ist gesetzt
