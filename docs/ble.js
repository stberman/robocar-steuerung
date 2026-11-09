/*
 * ble.js – Bluetooth-Verbindung zum Auto
 *
 * !!! Diese Datei muesst ihr normalerweise NICHT aendern. !!!
 * Eigene Bedienelemente baut ihr in controls.js ein.
 *
 * Die UUIDs und das Paketformat muessen zur Firmware passen
 * (firmware/RoboCar_BLE/config.h).
 */
'use strict';

const RC_UUID = {
  service: '6e8a1000-5c3b-4b6e-9a2f-0c1d2e3f4a50',
  control: '6e8a1001-5c3b-4b6e-9a2f-0c1d2e3f4a50',
  config:  '6e8a1002-5c3b-4b6e-9a2f-0c1d2e3f4a50',
  status:  '6e8a1003-5c3b-4b6e-9a2f-0c1d2e3f4a50',
};
const RC_PROTOCOL_VERSION = 1;
const RC_NAME_PREFIX = 'RoboCar-';
const RC_SEND_INTERVAL_MS = 40;      // 25 Pakete pro Sekunde
const RC_BTN = { BOOST: 0x01, IDENTIFY: 0x02 };
const RC_CFG = { INVERT_A: 0x01, INVERT_B: 0x02, SWAP_SIDES: 0x04, INVERT_STEER: 0x08 };

class CarLink {
  constructor() {
    // Das schreiben die Bedienelemente (controls.js). Werte:
    //   throttle, steer: -100..100   buttons: Bitmaske   aux: 4x 0..255
    this.state = { throttle: 0, steer: 0, buttons: 0, aux: [0, 0, 0, 0] };

    this.device = null;
    this.status = 'getrennt';  // 'getrennt' | 'verbinde' | 'verbunden' | 'neu verbinden'
    this.latencyMs = null;
    this.carInfo = null;       // { fw, group, failsafe }
    this.onUpdate = () => {};  // wird bei jeder Statusaenderung aufgerufen

    this._seq = 0;
    this._sentAt = new Array(256).fill(0);
    this._timer = null;
    this._queue = Promise.resolve();
    this._busy = 0;
    this._userDisconnect = false;
    this._identifyUntil = 0;
    this._ch = {};
  }

  static isSupported() {
    return !!(navigator.bluetooth && navigator.bluetooth.requestDevice);
  }

  get name() { return this.device ? this.device.name : ''; }

  // Browser-Dialog zur Auswahl des Autos oeffnen und verbinden
  async connect() {
    const device = await navigator.bluetooth.requestDevice({
      filters: [{ namePrefix: RC_NAME_PREFIX }],
      optionalServices: [RC_UUID.service],
    });
    if (this.device && this.device !== device) this.disconnect();
    this.device = device;
    this._userDisconnect = false;
    device.addEventListener('gattserverdisconnected', () => this._onDisconnected());
    this._setStatus('verbinde');
    await this._connectGatt();
  }

  disconnect() {
    this._userDisconnect = true;
    this._stopLoop();
    if (this.device && this.device.gatt.connected) this.device.gatt.disconnect();
    this._setStatus('getrennt');
  }

  // Alle Bedienelemente auf Null (z. B. wenn die App in den Hintergrund geht)
  stopAll() {
    this.state.throttle = 0;
    this.state.steer = 0;
    this.state.buttons = 0;
    this._sendNow();
  }

  // Laesst die blaue LED am Auto 3 Sekunden schnell blinken
  identify() { this._identifyUntil = performance.now() + 3000; }

  async readConfig() {
    const dv = await this._gatt(() => this._ch.config.readValue());
    return {
      group: dv.getUint8(1),
      maxSpeed: dv.getUint8(2),
      boostSpeed: dv.getUint8(3),
      trim: dv.getInt8(4),
      invertA: !!(dv.getUint8(5) & RC_CFG.INVERT_A),
      invertB: !!(dv.getUint8(5) & RC_CFG.INVERT_B),
      swapSides: !!(dv.getUint8(5) & RC_CFG.SWAP_SIDES),
      invertSteer: !!(dv.getUint8(5) & RC_CFG.INVERT_STEER),
      steerRate: dv.getUint8(6),
    };
  }

  async writeConfig(c) {
    const flags = (c.invertA ? RC_CFG.INVERT_A : 0) | (c.invertB ? RC_CFG.INVERT_B : 0) |
                  (c.swapSides ? RC_CFG.SWAP_SIDES : 0) | (c.invertSteer ? RC_CFG.INVERT_STEER : 0);
    const buf = new Uint8Array([RC_PROTOCOL_VERSION, c.group, c.maxSpeed, c.boostSpeed,
                                c.trim & 0xff, flags, c.steerRate, 0]);
    await this._gatt(() => this._ch.config.writeValue(buf));
  }

  // ---------------------------------------------------------------- intern

  async _connectGatt() {
    const server = await this.device.gatt.connect();
    const svc = await server.getPrimaryService(RC_UUID.service);
    this._ch.control = await svc.getCharacteristic(RC_UUID.control);
    this._ch.config = await svc.getCharacteristic(RC_UUID.config);
    this._ch.status = await svc.getCharacteristic(RC_UUID.status);
    this._ch.status.addEventListener('characteristicvaluechanged', (e) => this._onStatus(e.target.value));
    await this._ch.status.startNotifications();
    this._queue = Promise.resolve();
    this._busy = 0;
    this._setStatus('verbunden');
    this._startLoop();
  }

  async _onDisconnected() {
    this._stopLoop();
    this.latencyMs = null;
    if (this._userDisconnect) return;
    // Automatisch neu verbinden (z. B. Auto kurz ausser Reichweite oder Neustart nach Gruppenwechsel)
    this._setStatus('neu verbinden');
    for (let attempt = 1; attempt <= 8 && !this._userDisconnect; attempt++) {
      await new Promise((r) => setTimeout(r, 1000));
      if (this._userDisconnect) return;
      try {
        await this._connectGatt();
        return;
      } catch (e) {
        console.warn('Neu verbinden fehlgeschlagen', attempt, e);
      }
    }
    if (!this._userDisconnect) this._setStatus('getrennt');
  }

  _setStatus(s) {
    this.status = s;
    this.onUpdate();
  }

  // Alle GATT-Zugriffe nacheinander ausfuehren (der Browser erlaubt nur einen gleichzeitig)
  _gatt(fn) {
    this._busy++;
    const p = this._queue.then(fn).finally(() => { this._busy--; });
    this._queue = p.catch(() => {});
    return p;
  }

  _startLoop() {
    this._stopLoop();
    this._timer = setInterval(() => this._tick(), RC_SEND_INTERVAL_MS);
  }

  _stopLoop() {
    if (this._timer) clearInterval(this._timer);
    this._timer = null;
  }

  _tick() {
    // Wenn noch ein Zugriff laeuft, diesen Takt auslassen statt Pakete zu stauen
    if (this._busy > 0) return;
    this._sendNow();
  }

  _buildPacket() {
    const s = this.state;
    const clamp = (v, lo, hi) => Math.max(lo, Math.min(hi, Math.round(v) || 0));
    let buttons = s.buttons & 0xff;
    if (performance.now() < this._identifyUntil) buttons |= RC_BTN.IDENTIFY;
    this._seq = (this._seq + 1) & 0xff;
    this._sentAt[this._seq] = performance.now();
    return new Uint8Array([
      RC_PROTOCOL_VERSION,
      this._seq,
      clamp(s.throttle, -100, 100) & 0xff,
      clamp(s.steer, -100, 100) & 0xff,
      buttons,
      clamp(s.aux[0], 0, 255), clamp(s.aux[1], 0, 255),
      clamp(s.aux[2], 0, 255), clamp(s.aux[3], 0, 255),
    ]);
  }

  _sendNow() {
    if (this.status !== 'verbunden' || !this._ch.control) return;
    const pkt = this._buildPacket();
    const ch = this._ch.control;
    const write = ch.writeValueWithoutResponse ? () => ch.writeValueWithoutResponse(pkt)
                                               : () => ch.writeValue(pkt);
    this._gatt(write).catch((e) => console.warn('Senden fehlgeschlagen', e));
  }

  _onStatus(dv) {
    if (dv.byteLength < 6) return;
    const seq = dv.getUint8(2);
    const sent = this._sentAt[seq];
    if (sent) {
      const rtt = performance.now() - sent;
      this.latencyMs = this.latencyMs == null ? rtt : this.latencyMs * 0.8 + rtt * 0.2;
    }
    this.carInfo = {
      fw: `${dv.getUint8(0)}.${dv.getUint8(1)}`,
      failsafe: !!(dv.getUint8(3) & 0x01),
      group: dv.getUint8(4),
    };
    this.onUpdate();
  }
}

// Eine gemeinsame Verbindung fuer die ganze App
const car = new CarLink();
