/*
 * app.js – Ansichten umschalten, Statusanzeige, Bildschirm wach halten
 */
'use strict';

const App = {
  wakeLock: null,
  // Demo-Modus (Adresse mit ?demo): Fahransicht ohne Auto, zeigt die Werte an,
  // die gesendet wuerden. Praktisch zum Testen eigener Aenderungen in controls.js.
  demo: new URLSearchParams(location.search).has('demo'),

  init() {
    if (this.demo) setInterval(() => this.render(), 100);

    if (!CarLink.isSupported()) {
      document.getElementById('unsupported').hidden = false;
      document.getElementById('btn-connect').disabled = true;
    }

    document.getElementById('btn-connect').addEventListener('click', () => this.connect());
    document.getElementById('btn-disconnect').addEventListener('click', () => {
      Controls.reset();
      car.stopAll();
      car.disconnect();
    });

    car.onUpdate = () => this.render();

    // App in den Hintergrund (Anruf, Home-Taste): sofort anhalten
    document.addEventListener('visibilitychange', () => {
      if (document.hidden) {
        Controls.reset();
        car.stopAll();
      } else {
        this.keepAwake();
      }
    });
    window.addEventListener('resize', () => Controls.reset());

    Controls.init();
    Settings.init();
    this.render();

    if ('serviceWorker' in navigator) {
      navigator.serviceWorker.register('sw.js').catch(() => {});
    }
  },

  async connect() {
    const msg = document.getElementById('connect-msg');
    msg.textContent = '';
    try {
      this.fullscreen();
      await car.connect();
      this.keepAwake();
    } catch (e) {
      if (e.name === 'NotFoundError') msg.textContent = 'Kein Auto ausgewählt.';
      else msg.textContent = 'Verbindung fehlgeschlagen: ' + e.message;
      this.render();
    }
  },

  render() {
    const drive = this.demo || car.status === 'verbunden' || car.status === 'neu verbinden';
    const wasDrive = !document.getElementById('view-drive').hidden;
    document.getElementById('view-connect').hidden = drive;
    document.getElementById('view-drive').hidden = !drive;
    if (drive && !wasDrive) Controls.reset();   // Joysticks neu ausrichten

    document.getElementById('car-name').textContent = car.name || '–';
    const dot = document.getElementById('status-dot');
    dot.dataset.status = car.status;
    dot.title = car.status;

    let info = car.status === 'neu verbinden' ? 'Verbindung verloren – verbinde neu …' : '';
    if (car.status === 'verbunden' && car.latencyMs != null) info = `${Math.round(car.latencyMs)} ms`;
    if (car.carInfo && car.carInfo.failsafe && car.status === 'verbunden') info += ' · gestoppt';
    if (this.demo) {
      const s = car.state;
      document.getElementById('car-name').textContent = 'DEMO';
      info = `Gas ${s.throttle} · Lenkung ${s.steer} · Tasten ${s.buttons} · Aux ${s.aux.join('/')}`;
    }
    document.getElementById('car-info').textContent = info;

    if (car.status === 'neu verbinden') Controls.reset();
  },

  // Bildschirm soll waehrend des Spiels nicht ausgehen
  async keepAwake() {
    try {
      if ('wakeLock' in navigator && !document.hidden) {
        this.wakeLock = await navigator.wakeLock.request('screen');
      }
    } catch (e) { /* nicht schlimm */ }
  },

  fullscreen() {
    const el = document.documentElement;
    if (document.fullscreenElement || !el.requestFullscreen) return;
    el.requestFullscreen({ navigationUI: 'hide' })
      .then(() => screen.orientation && screen.orientation.lock && screen.orientation.lock('landscape'))
      .catch(() => {});
  },
};

window.addEventListener('DOMContentLoaded', () => App.init());
