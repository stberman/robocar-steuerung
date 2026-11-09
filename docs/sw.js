/*
 * sw.js – Service Worker: App funktioniert auch ohne Internet.
 * Strategie "erst Netz, dann Cache": Aenderungen am Code sind sofort sichtbar,
 * ohne Netz wird die zuletzt geladene Version verwendet.
 */
const CACHE = 'robocar-v3';
const FILES = ['./', 'index.html', 'style.css', 'ble.js', 'controls.js', 'settings.js', 'app.js',
               'manifest.json', 'icon.svg'];

self.addEventListener('install', (e) => {
  e.waitUntil(caches.open(CACHE).then((c) => c.addAll(FILES)).then(() => self.skipWaiting()));
});

self.addEventListener('activate', (e) => {
  e.waitUntil(
    caches.keys()
      .then((keys) => Promise.all(keys.filter((k) => k !== CACHE).map((k) => caches.delete(k))))
      .then(() => self.clients.claim())
  );
});

self.addEventListener('fetch', (e) => {
  if (e.request.method !== 'GET') return;
  e.respondWith(
    fetch(e.request)
      .then((res) => {
        const copy = res.clone();
        caches.open(CACHE).then((c) => c.put(e.request, copy));
        return res;
      })
      .catch(() => caches.match(e.request))
  );
});
