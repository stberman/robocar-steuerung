/*
 * controls.js – Bedienelemente der App
 *
 * ================================================================
 *  HIER DUERFT IHR AENDERN!
 * ================================================================
 * Am einfachsten passt ihr nur die Einstellungen in AUX_KANAELE an:
 * Beschriftung und Art jedes Aux-Kanals. Die Werte kommen in der
 * Firmware in aux_user.cpp -> onAux(kanal, wert) an.
 *
 * Arten:
 *   'taster'   gedrueckt = 255, losgelassen = 0   (z. B. Kicker)
 *   'schalter' jedes Antippen wechselt 0 <-> 255  (z. B. Licht)
 *   'schieber' 0..255 stufenlos                   (z. B. Servo-Winkel)
 *   null       Kanal wird nicht angezeigt
 */
'use strict';

const AUX_KANAELE = [
  { name: 'KICK',  art: 'taster'   },   // AUX 1
  { name: 'LICHT', art: 'schalter' },   // AUX 2
  { name: 'ARM',   art: 'schieber' },   // AUX 3
  { name: 'AUX 4', art: null       },   // AUX 4
];

// Fahrmodi:
//   'joystick'  ein Joystick links: hoch/runter = Gas, links/rechts = Lenken
//   'zweihand'  links nur Gas (hoch/runter), rechts nur Lenken (links/rechts)
//   'panzer'    links = linke Seite, rechts = rechte Seite (je hoch/runter)
const FAHRMODI = ['joystick', 'zweihand', 'panzer'];

// ------------------------------------------------------------------
// Joystick: Wo der Daumen aufsetzt, ist die Mitte. Liefert x, y in -1..1
// (y positiv = nach oben). achsen: 'xy', 'x' oder 'y'.
// ------------------------------------------------------------------
class Stick {
  constructor(zone, achsen) {
    this.zone = zone;
    this.achsen = achsen;
    this.x = 0;
    this.y = 0;
    this.radius = 70;
    this.pointerId = null;

    this.base = document.createElement('div');
    this.base.className = 'stick-base';
    this.knob = document.createElement('div');
    this.knob.className = 'stick-knob';
    this.base.appendChild(this.knob);
    zone.appendChild(this.base);
    this._showIdle();

    zone.addEventListener('pointerdown', (e) => this._down(e));
    zone.addEventListener('pointermove', (e) => this._move(e));
    zone.addEventListener('pointerup', (e) => this._up(e));
    zone.addEventListener('pointercancel', (e) => this._up(e));
  }

  setAchsen(achsen) { this.achsen = achsen; this.zone.dataset.achsen = achsen; this.reset(); }

  reset() {
    this.x = 0;
    this.y = 0;
    this.pointerId = null;
    this._showIdle();
  }

  _showIdle() {
    const r = this.zone.getBoundingClientRect();
    this._placeBase(r.width / 2, r.height / 2);
    this.knob.style.transform = 'translate(-50%, -50%)';
    this.base.classList.remove('active');
  }

  _placeBase(x, y) {
    this.cx = x;
    this.cy = y;
    this.base.style.left = x + 'px';
    this.base.style.top = y + 'px';
  }

  _down(e) {
    if (this.pointerId !== null) return;
    this.pointerId = e.pointerId;
    this.zone.setPointerCapture(e.pointerId);
    const r = this.zone.getBoundingClientRect();
    this._placeBase(e.clientX - r.left, e.clientY - r.top);
    this.base.classList.add('active');
    this._move(e);
  }

  _move(e) {
    if (e.pointerId !== this.pointerId) return;
    const r = this.zone.getBoundingClientRect();
    let dx = e.clientX - r.left - this.cx;
    let dy = e.clientY - r.top - this.cy;
    if (this.achsen === 'x') dy = 0;
    if (this.achsen === 'y') dx = 0;
    const len = Math.hypot(dx, dy);
    if (len > this.radius) { dx *= this.radius / len; dy *= this.radius / len; }
    this.knob.style.transform = `translate(calc(-50% + ${dx}px), calc(-50% + ${dy}px))`;
    this.x = dx / this.radius;
    this.y = -dy / this.radius;
  }

  _up(e) {
    if (e.pointerId !== this.pointerId) return;
    this.reset();
  }
}

// ------------------------------------------------------------------
// Aufbau der Bedienoberflaeche
// ------------------------------------------------------------------
const Controls = {
  modus: 'joystick',
  links: null,
  rechts: null,
  boost: false,
  aux: [0, 0, 0, 0],

  init() {
    this.links = new Stick(document.getElementById('zone-left'), 'xy');
    this.rechts = new Stick(document.getElementById('zone-right'), 'x');

    // Boost-Taste (gedrueckt halten)
    const boost = document.getElementById('btn-boost');
    this._halten(boost, (an) => { this.boost = an; });

    // Aux-Bedienelemente erzeugen
    const box = document.getElementById('aux-box');
    AUX_KANAELE.forEach((k, i) => {
      if (!k || !k.art) return;
      if (k.art === 'taster') {
        const b = this._button(k.name);
        this._halten(b, (an) => { this.aux[i] = an ? 255 : 0; });
        box.appendChild(b);
      } else if (k.art === 'schalter') {
        const b = this._button(k.name);
        b.addEventListener('click', () => {
          this.aux[i] = this.aux[i] ? 0 : 255;
          b.classList.toggle('on', this.aux[i] > 0);
        });
        box.appendChild(b);
      } else if (k.art === 'schieber') {
        const label = document.createElement('label');
        label.className = 'aux-slider';
        label.textContent = k.name;
        const s = document.createElement('input');
        s.type = 'range'; s.min = 0; s.max = 255; s.value = 0;
        s.addEventListener('input', () => { this.aux[i] = +s.value; });
        label.appendChild(s);
        box.appendChild(label);
      }
    });

    // Fahrmodus umschalten (wird im Browser gespeichert)
    let gespeichert = null;
    try { gespeichert = localStorage.getItem('fahrmodus'); } catch (e) { /* egal */ }
    this.setModus(FAHRMODI.includes(gespeichert) ? gespeichert : 'joystick');
    document.getElementById('btn-mode').addEventListener('click', () => {
      const next = FAHRMODI[(FAHRMODI.indexOf(this.modus) + 1) % FAHRMODI.length];
      this.setModus(next);
    });

    requestAnimationFrame(() => this._frame());
  },

  setModus(m) {
    this.modus = m;
    try { localStorage.setItem('fahrmodus', m); } catch (e) { /* egal */ }
    document.body.dataset.modus = m;
    document.getElementById('btn-mode').textContent =
      { joystick: 'Modus: Joystick', zweihand: 'Modus: 2 Hände', panzer: 'Modus: Panzer' }[m];
    this.links.setAchsen(m === 'joystick' ? 'xy' : 'y');
    this.rechts.setAchsen(m === 'zweihand' ? 'x' : 'y');
  },

  // Alles loslassen (App im Hintergrund, Verbindung weg)
  reset() {
    this.links.reset();
    this.rechts.reset();
    this.boost = false;
    document.querySelectorAll('.hold').forEach((b) => b.classList.remove('on'));
    AUX_KANAELE.forEach((k, i) => { if (k && k.art === 'taster') this.aux[i] = 0; });
  },

  _button(text) {
    const b = document.createElement('button');
    b.className = 'aux-btn';
    b.textContent = text;
    return b;
  },

  // Taste, die nur aktiv ist, solange sie gedrueckt wird
  _halten(el, setze) {
    el.classList.add('hold');
    const an = (e) => { e.preventDefault(); el.setPointerCapture(e.pointerId); el.classList.add('on'); setze(true); };
    const aus = () => { el.classList.remove('on'); setze(false); };
    el.addEventListener('pointerdown', an);
    el.addEventListener('pointerup', aus);
    el.addEventListener('pointercancel', aus);
  },

  // Laeuft mit der Bildwiederholrate: Bedienelemente -> car.state
  _frame() {
    let gas, lenk;
    if (this.modus === 'panzer') {
      // linke/rechte Seite in Gas + Lenkung umrechnen
      const l = this.links.y, r = this.rechts.y;
      gas = (l + r) / 2;
      lenk = (l - r) / 2;
    } else if (this.modus === 'zweihand') {
      gas = this.links.y;
      lenk = this.rechts.x;
    } else {
      gas = this.links.y;
      lenk = this.links.x;
    }
    let buttons = this.boost ? RC_BTN.BOOST : 0;
    const aux = this.aux.slice();

    // Optional: Gamecontroller am Handy (Bluetooth/USB). Er uebernimmt,
    // sobald er bewegt wird und der Touchscreen nicht benutzt wird.
    const pad = this._gamepad();
    if (pad && gas === 0 && lenk === 0) {
      const dz = (v) => (Math.abs(v) < 0.12 ? 0 : v);
      gas = -dz(pad.axes[1] || 0);
      lenk = dz(pad.axes[0] || 0);
      if (pad.buttons[7] && pad.buttons[7].pressed) buttons |= RC_BTN.BOOST;  // rechter Trigger
      for (let i = 0; i < 4; i++) {                                           // A, B, X, Y
        if (pad.buttons[i] && pad.buttons[i].pressed) aux[i] = 255;
      }
    }

    car.state.throttle = Math.round(gas * 100);
    car.state.steer = Math.round(lenk * 100);
    car.state.buttons = buttons;
    car.state.aux = aux;

    requestAnimationFrame(() => this._frame());
  },

  _gamepad() {
    if (!navigator.getGamepads) return null;
    for (const p of navigator.getGamepads()) {
      if (p && p.connected) return p;
    }
    return null;
  },
};
