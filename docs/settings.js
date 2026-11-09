/*
 * settings.js – Einstellungen des Autos (werden im Auto gespeichert)
 */
'use strict';

const Settings = {
  dialog: null,
  form: null,
  loadedGroup: 0,

  init() {
    this.dialog = document.getElementById('settings');
    this.form = document.getElementById('settings-form');

    // Anzeige der Schieberwerte
    this.form.querySelectorAll('input[type=range]').forEach((r) => {
      const out = this.form.querySelector(`output[for=${r.id}]`);
      const show = () => { if (out) out.textContent = r.value + (r.dataset.unit || ''); };
      r.addEventListener('input', show);
      r._show = show;
    });

    document.getElementById('btn-settings').addEventListener('click', () => this.open());
    document.getElementById('btn-settings-close').addEventListener('click', () => this.dialog.close());
    document.getElementById('btn-identify').addEventListener('click', () => car.identify());
    this.form.addEventListener('submit', (e) => { e.preventDefault(); this.save(); });
  },

  async open() {
    Controls.reset();
    this._msg('Lade Einstellungen …');
    this.dialog.showModal();
    try {
      const c = await car.readConfig();
      this.loadedGroup = c.group;
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
      this.form.querySelectorAll('input[type=range]').forEach((r) => r._show());
      this._msg('');
    } catch (e) {
      this._msg('Konnte Einstellungen nicht lesen: ' + e.message);
    }
  },

  async save() {
    const f = this.form.elements;
    const group = parseInt(f.group.value, 10) || 0;
    if (group < 0 || group > 16) { this._msg('Gruppe muss zwischen 1 und 16 liegen.'); return; }
    const c = {
      group,
      maxSpeed: +f.maxSpeed.value,
      boostSpeed: +f.boostSpeed.value,
      trim: +f.trim.value,
      steerRate: +f.steerRate.value,
      invertA: f.invertA.checked,
      invertB: f.invertB.checked,
      swapSides: f.swapSides.checked,
      invertSteer: f.invertSteer.checked,
    };
    try {
      await car.writeConfig(c);
      if (group !== this.loadedGroup) {
        this._msg(`Gespeichert. Das Auto startet neu und heißt danach RoboCar-${String(group).padStart(2, '0')}. Die App verbindet sich automatisch neu.`);
        this.loadedGroup = group;
      } else {
        this._msg('Gespeichert ✓');
      }
    } catch (e) {
      this._msg('Speichern fehlgeschlagen: ' + e.message);
    }
  },

  _msg(t) { document.getElementById('settings-msg').textContent = t; },
};
