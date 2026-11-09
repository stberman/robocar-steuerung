/*
 * settings.js – Einstellungen des Autos (werden im Auto gespeichert)
 *
 * Reiter "Allgemein":   Aenderungen werden mit "Speichern" uebertragen.
 * Reiter "Kalibrieren": Aenderungen wirken sofort (werden automatisch
 *                       uebertragen und gespeichert). Test-Tasten fahren das
 *                       Auto, solange sie gedrueckt sind.
 */
'use strict';

// Standardwerte der Kalibrierung (muessen zu firmware/RoboCar_BLE/config.h passen)
const KALIB_STANDARD = { minStart: 35, trim: 0, curveFull: 50, expo: 0, deadzone: 5, ramp: 13 };
const KALIB_FELDER = ['minStart', 'curveFull', 'ramp', 'deadzone', 'expo'];

const Settings = {
  dialog: null,
  form: null,
  loadedGroup: 0,
  hasCalib: false,
  _liveTimer: null,
  // Im Demo-Modus (ohne Auto) werden die Einstellungen nur hier gehalten
  _demoConfig: {
    group: 0, maxSpeed: 70, boostSpeed: 100, trim: 0, steerRate: 70,
    invertA: false, invertB: false, swapSides: false, invertSteer: false,
    calib: { minStart: 35, curveFull: 50, ramp: 13, deadzone: 5, expo: 0 },
  },

  init() {
    this.dialog = document.getElementById('settings');
    this.form = document.getElementById('settings-form');

    // Anzeige der Schieberwerte; Kalibrier-Schieber (data-live) wirken sofort
    this.form.querySelectorAll('input[type=range]').forEach((r) => {
      const out = this.form.querySelector(`output[for=${r.id}]`);
      const show = () => {
        if (out) out.textContent = r.value * (r.dataset.scale || 1) + (r.dataset.unit || '');
      };
      r.addEventListener('input', () => {
        show();
        if ('live' in r.dataset) this._liveSend();
      });
      r._show = show;
    });

    // Reiter umschalten
    this.form.querySelectorAll('.tab').forEach((t) => {
      t.addEventListener('click', () => this._showTab(t.dataset.tab));
    });

    // Test-Tasten: Das Auto faehrt, solange die Taste gedrueckt ist
    this.form.querySelectorAll('.test-btn').forEach((b) => {
      const an = (e) => {
        e.preventDefault();
        b.setPointerCapture(e.pointerId);
        b.classList.add('on');
        // "Anlauf": knapp ueber dem Totbereich -> Motoren laufen mit der Anlaufschwelle
        const gas = b.dataset.gas === 'anlauf' ? +this.form.elements.deadzone.value + 2 : +b.dataset.gas;
        Controls.test = { gas: gas / 100, lenk: +b.dataset.lenk / 100 };
      };
      const aus = () => { b.classList.remove('on'); Controls.test = null; };
      b.addEventListener('pointerdown', an);
      b.addEventListener('pointerup', aus);
      b.addEventListener('pointercancel', aus);
      b.addEventListener('lostpointercapture', aus);
      b.addEventListener('contextmenu', (e) => e.preventDefault());
    });

    document.getElementById('btn-kalib-default').addEventListener('click', () => {
      const f = this.form.elements;
      for (const [k, v] of Object.entries(KALIB_STANDARD)) f[k].value = v;
      this.form.querySelectorAll('input[type=range]').forEach((r) => r._show());
      this._liveSend();
    });

    document.getElementById('btn-settings').addEventListener('click', () => this.open());
    document.getElementById('btn-settings-close').addEventListener('click', () => this.dialog.close());
    this.dialog.addEventListener('close', () => { Controls.test = null; });
    document.getElementById('btn-identify').addEventListener('click', () => car.identify());
    this.form.addEventListener('submit', (e) => { e.preventDefault(); this.save(); });
  },

  async open() {
    Controls.reset();
    this._msg('Lade Einstellungen …');
    this.dialog.showModal();
    try {
      const c = App.demo ? JSON.parse(JSON.stringify(this._demoConfig)) : await car.readConfig();
      this.loadedGroup = c.group;
      this.hasCalib = !!c.calib;
      const f = this.form.elements;
      f.group.value = c.group || '';
      f.maxSpeed.value = c.maxSpeed;
      f.boostSpeed.value = c.boostSpeed;
      f.trim.value = c.trim;
      f.steerRate.value = c.steerRate;
      f.invertA.checked = c.invertA;
      f.invertB.checked = c.invertB;
      f.swapSides.checked = c.swapSides;
      f.invertSteer.checked = c.invertSteer;
      if (c.calib) KALIB_FELDER.forEach((k) => { f[k].value = c.calib[k]; });

      // Alte Firmware (nur 8-Byte-Paket): Kalibrier-Regler sperren, Trim geht trotzdem
      document.getElementById('kalib-old').hidden = this.hasCalib;
      KALIB_FELDER.forEach((k) => { f[k].disabled = !this.hasCalib; });

      this.form.querySelectorAll('input[type=range]').forEach((r) => r._show());
      this._msg('');
    } catch (e) {
      this._msg('Konnte Einstellungen nicht lesen: ' + e.message);
    }
  },

  // Aktuelle Formularwerte als Konfiguration
  _collect(group) {
    const f = this.form.elements;
    const calib = {};
    KALIB_FELDER.forEach((k) => { calib[k] = +f[k].value; });
    return {
      group,
      maxSpeed: +f.maxSpeed.value,
      boostSpeed: +f.boostSpeed.value,
      trim: +f.trim.value,
      steerRate: +f.steerRate.value,
      invertA: f.invertA.checked,
      invertB: f.invertB.checked,
      swapSides: f.swapSides.checked,
      invertSteer: f.invertSteer.checked,
      calib: this.hasCalib ? calib : null,
    };
  },

  async _write(c) {
    if (App.demo) { this._demoConfig = c; return; }
    await car.writeConfig(c);
  },

  async save() {
    const group = parseInt(this.form.elements.group.value, 10) || 0;
    if (group < 0 || group > 16) { this._msg('Gruppe muss zwischen 1 und 16 liegen.'); return; }
    try {
      await this._write(this._collect(group));
      if (group !== this.loadedGroup) {
        const eigenerName = car.name && !car.name.startsWith(RC_NAME_PREFIX);
        const neuerName = eigenerName ? car.name : `RoboCar-${String(group).padStart(2, '0')}`;
        this._msg(`Gespeichert. Das Auto startet neu und heißt danach ${neuerName}. Die App verbindet sich automatisch neu.`);
        this.loadedGroup = group;
      } else {
        this._msg('Gespeichert ✓');
      }
    } catch (e) {
      this._msg('Speichern fehlgeschlagen: ' + e.message);
    }
  },

  // Kalibrierung sofort uebertragen (kurz verzoegert, damit beim Schieben
  // nicht jede Zwischenstellung gesendet wird). Gesendet wird die bisher
  // gespeicherte Gruppe, damit das Auto dabei nicht neu startet.
  _liveSend() {
    clearTimeout(this._liveTimer);
    this._liveTimer = setTimeout(async () => {
      try {
        await this._write(this._collect(this.loadedGroup));
        this._msg('Übernommen ✓');
      } catch (e) {
        this._msg('Übertragen fehlgeschlagen: ' + e.message);
      }
    }, 250);
  },

  _showTab(name) {
    this.form.querySelectorAll('.tab').forEach((t) => t.setAttribute('aria-selected', t.dataset.tab === name));
    this.form.querySelectorAll('.tab-panel').forEach((p) => { p.hidden = p.dataset.panel !== name; });
    Controls.test = null;
    this._msg('');
  },

  _msg(t) { document.getElementById('settings-msg').textContent = t; },
};
