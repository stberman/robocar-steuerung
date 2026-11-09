#include "settings.h"
#include "config.h"
#include <Preferences.h>
#include <esp_mac.h>

Settings settings;
static Preferences prefs;

static const Settings DEFAULTS = {
  /* group      */ 0,
  /* maxSpeed   */ 70,
  /* boostSpeed */ 100,
  /* trim       */ 0,
  /* flags      */ 0,
  /* steerRate  */ 70,
};

static uint8_t clampU8(int v, int lo, int hi) { return (uint8_t)constrain(v, lo, hi); }

static void sanitize() {
  settings.group      = clampU8(settings.group, 0, 16);
  settings.maxSpeed   = clampU8(settings.maxSpeed, 10, 100);
  settings.boostSpeed = clampU8(settings.boostSpeed, 10, 100);
  settings.trim       = (int8_t)constrain(settings.trim, -50, 50);
  settings.flags     &= (CFG_INVERT_A | CFG_INVERT_B | CFG_SWAP_SIDES | CFG_INVERT_STEER);
  settings.steerRate  = clampU8(settings.steerRate, 20, 100);
}

void settingsLoad() {
  settings = DEFAULTS;
  prefs.begin("robocar", true);
  if (prefs.getBytesLength("cfg") == sizeof(Settings)) {
    prefs.getBytes("cfg", &settings, sizeof(Settings));
  }
  prefs.end();
  sanitize();
}

void settingsSave() {
  prefs.begin("robocar", false);
  prefs.putBytes("cfg", &settings, sizeof(Settings));
  prefs.end();
}

void settingsToBytes(uint8_t* out) {
  out[0] = PROTOCOL_VERSION;
  out[1] = settings.group;
  out[2] = settings.maxSpeed;
  out[3] = settings.boostSpeed;
  out[4] = (uint8_t)settings.trim;
  out[5] = settings.flags;
  out[6] = settings.steerRate;
  out[7] = 0;
}

bool settingsFromBytes(const uint8_t* in, size_t len) {
  if (len < CONFIG_LEN || in[0] != PROTOCOL_VERSION) return false;
  settings.group      = in[1];
  settings.maxSpeed   = in[2];
  settings.boostSpeed = in[3];
  settings.trim       = (int8_t)in[4];
  settings.flags      = in[5];
  settings.steerRate  = in[6];
  sanitize();
  return true;
}

String deviceName() {
  char buf[20];
  if (settings.group >= 1 && settings.group <= 16) {
    snprintf(buf, sizeof(buf), "%s%02u", BLE_NAME_PREFIX, settings.group);
  } else {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_BT);
    snprintf(buf, sizeof(buf), "%s%02X%02X", BLE_NAME_PREFIX, mac[4], mac[5]);
  }
  return String(buf);
}
