/*
 * settings.cpp – Einstellungen laden, speichern und mit der App austauschen
 */
#include "settings.h"
#include "config.h"
#include <Preferences.h>   // Bibliothek zum dauerhaften Speichern im Flash (ist beim ESP32 dabei)
#include <esp_mac.h>       // zum Auslesen der Gerätenummer (MAC-Adresse)

Settings settings;           // hier wird die gemeinsame Variable tatsächlich angelegt
static Preferences prefs;

// Werte für ein ganz neues Auto (noch nie etwas in der App eingestellt)
static const Settings DEFAULTS = {
  /* group      */ 0,
  /* maxSpeed   */ 70,
  /* boostSpeed */ 100,
  /* trim       */ 0,
  /* flags      */ 0,
  /* steerRate  */ 70,
  /* minStart   */ DEF_MIN_START_PCT,
  /* curveFull  */ DEF_CURVE_FULL_PCT,
  /* ramp       */ DEF_RAMP_10MS,
  /* deadzone   */ DEF_DEADZONE_PCT,
  /* expo       */ DEF_EXPO_PCT,
};

// Zahl auf den Bereich lo..hi begrenzen
static uint8_t clampU8(int v, int lo, int hi) { return (uint8_t)constrain(v, lo, hi); }

// Alle Werte auf ihren erlaubten Bereich begrenzen. So kann ein fehlerhaftes
// Datenpaket das Auto nicht in einen unsinnigen Zustand bringen.
static void sanitize() {
  settings.group      = clampU8(settings.group, 0, 16);
  settings.maxSpeed   = clampU8(settings.maxSpeed, 10, 100);
  settings.boostSpeed = clampU8(settings.boostSpeed, 10, 100);
  settings.trim       = (int8_t)constrain(settings.trim, -50, 50);
  settings.flags     &= (CFG_INVERT_A | CFG_INVERT_B | CFG_SWAP_SIDES | CFG_INVERT_STEER);  // nur bekannte Bits behalten
  settings.steerRate  = clampU8(settings.steerRate, 20, 100);
  settings.minStart   = clampU8(settings.minStart, 0, 60);
  settings.curveFull  = clampU8(settings.curveFull, 20, 100);
  settings.ramp       = clampU8(settings.ramp, 0, 50);
  settings.deadzone   = clampU8(settings.deadzone, 0, 20);
  settings.expo       = clampU8(settings.expo, 0, 100);
}

void settingsLoad() {
  settings = DEFAULTS;                     // erst Standardwerte ...
  prefs.begin("robocar", true);            // Speicherbereich "robocar" zum Lesen öffnen
  // ... dann die gespeicherten Werte darüberlegen (falls vorhanden).
  // Neue Felder stehen hinten im struct. Ein kürzerer Datensatz aus einer
  // älteren Firmware wird übernommen, der Rest bleibt auf Standardwerten.
  size_t n = prefs.getBytesLength("cfg");
  if (n > 0 && n <= sizeof(Settings)) {
    prefs.getBytes("cfg", &settings, n);
  }
  prefs.end();
  sanitize();
}

void settingsSave() {
  prefs.begin("robocar", false);           // zum Schreiben öffnen
  prefs.putBytes("cfg", &settings, sizeof(Settings));
  prefs.end();
}

// Aufbau des Pakets: siehe "Konfiguration" in config.h
void settingsToBytes(uint8_t* out) {
  out[0] = PROTOCOL_VERSION;
  out[1] = settings.group;
  out[2] = settings.maxSpeed;
  out[3] = settings.boostSpeed;
  out[4] = (uint8_t)settings.trim;         // negative Zahl als Byte übertragen
  out[5] = settings.flags;
  out[6] = settings.steerRate;
  out[7] = 0;
  out[8]  = settings.minStart;
  out[9]  = settings.curveFull;
  out[10] = settings.ramp;
  out[11] = settings.deadzone;
  out[12] = settings.expo;
  out[13] = out[14] = out[15] = 0;
}

bool settingsFromBytes(const uint8_t* in, size_t len) {
  // Zu kurz oder falsche Protokollversion -> ignorieren
  if (len < CONFIG_LEN_V1 || in[0] != PROTOCOL_VERSION) return false;
  settings.group      = in[1];
  settings.maxSpeed   = in[2];
  settings.boostSpeed = in[3];
  settings.trim       = (int8_t)in[4];
  settings.flags      = in[5];
  settings.steerRate  = in[6];
  // Kalibrierung nur übernehmen, wenn die App sie mitschickt (neuere App)
  if (len >= CONFIG_LEN) {
    settings.minStart  = in[8];
    settings.curveFull = in[9];
    settings.ramp      = in[10];
    settings.deadzone  = in[11];
    settings.expo      = in[12];
  }
  sanitize();
  return true;
}

static String customName;   // eigener Name aus der .ino (ohne "RoboCar-")

void settingsSetCustomName(const char* name) {
  customName = name ? name : "";
  customName.trim();                       // Leerzeichen am Anfang/Ende entfernen
  // Wer "RoboCar-" selbst mitschreibt, bekommt es nicht doppelt
  if (customName.startsWith(BLE_NAME_PREFIX)) customName = customName.substring(strlen(BLE_NAME_PREFIX));
  // Der Name muss in ein Bluetooth-Paket passen (31 Byte inkl. Verwaltungsdaten):
  // "RoboCar-" + 18 Zeichen = 26 Byte. Längere Namen werden abgeschnitten.
  if (customName.length() > 18) customName = customName.substring(0, 18);
}

String deviceName() {
  // "RoboCar-" steht immer vorne, daran erkennt die App die Autos
  if (customName.length() > 0) return String(BLE_NAME_PREFIX) + customName;

  char buf[20];
  if (settings.group >= 1 && settings.group <= 16) {
    // %02u = Zahl immer zweistellig, also "07" statt "7"
    snprintf(buf, sizeof(buf), "%s%02u", BLE_NAME_PREFIX, settings.group);
  } else {
    // Noch keine Gruppe: die letzten 4 Stellen der Gerätenummer (MAC) anhängen,
    // damit sich neue Autos trotzdem unterscheiden lassen
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_BT);
    snprintf(buf, sizeof(buf), "%s%02X%02X", BLE_NAME_PREFIX, mac[4], mac[5]);
  }
  return String(buf);
}
