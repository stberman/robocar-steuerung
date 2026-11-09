# RoboCar Steuerung per Handy (Bluetooth LE) – WS26/27

**App:** https://stberman.github.io/robocar-steuerung/  ·  Demo ohne Auto: https://stberman.github.io/robocar-steuerung/?demo

Neue Steuerung für die Projektwoche: Jede Gruppe steuert ihr Auto **direkt mit dem eigenen Handy** per Bluetooth.
Raspberry Pis, WLAN-Access-Point, MQTT-Broker und USB-Gamepads aus WS25/26 (`WS25_26/Program/Auto_soccer/`) entfallen.

```
 Handy (Web-App im Browser)  ──BLE──▶  ESP32  ──▶  L298N  ──▶  Motoren
   Joystick, Boost, AUX 1–4   ◀─Status─  (eine Verbindung pro Auto)
```

| Ordner | Inhalt |
|---|---|
| `firmware/RoboCar_BLE/` | Arduino-Sketch für den ESP32 (identisch für alle Autos) |
| `docs/` | Web-App (HTML/CSS/JS). Läuft im Browser und lässt sich als App installieren |

---

## 1. Firmware flashen (Betreuer, einmal pro ESP32)

1. Arduino IDE 2.x installieren.
2. Boardverwalter: **„esp32 by Espressif Systems“ Version 3.x** installieren (getestet mit 3.3.12).
3. Bibliotheksverwalter: **„NimBLE-Arduino“ von h2zero, Version 2.x** installieren (getestet mit 2.5.2).
4. `firmware/RoboCar_BLE/RoboCar_BLE.ino` öffnen, Board **„ESP32 Dev Module“** wählen, hochladen.

Alle Autos bekommen dasselbe Programm. Nach dem ersten Start heißt das Auto `RoboCar-XXXX` (aus der MAC-Adresse).
Die Gruppennummer wird in der App gesetzt (⚙ → Gruppe). Danach startet das Auto neu und heißt `RoboCar-07`.

Serieller Monitor (115200 Baud) zeigt Verbindung, Failsafe, gespeicherte Einstellungen und AUX-Werte.

Kompiliert mit arduino-cli (`esp32:esp32:esp32`): 604 kB Flash (46 %), ohne Warnungen.

## 2. App bereitstellen

Web Bluetooth funktioniert nur über **https** (oder `localhost`). Der Inhalt von `docs/` muss also auf einem https-Server liegen:

- **GitHub Pages** (empfohlen): Repository anlegen, Inhalt von `docs/` hochladen, Pages aktivieren.
  Gruppen, die die App ändern wollen, forken das Repository und aktivieren in ihrem Fork ebenfalls Pages.
- alternativ ein TH-Webserver.

Die Adresse als **QR-Code** ausdrucken (Raum-Aushang, Anleitung).

| Handy | So geht's |
|---|---|
| Android | Seite in **Chrome** öffnen. Menü → „Zum Startbildschirm hinzufügen“ installiert die App |
| iPhone | Safari kann kein Web Bluetooth. Kostenlose App **„Bluefy – Web BLE Browser“** installieren und die Seite darin öffnen |

**Demo-Modus:** Wer `?demo` an die Adresse hängt (`…/index.html?demo`), sieht die Fahransicht ohne Auto. Oben werden die Werte angezeigt, die gesendet würden. Damit können die Studierenden ihre Änderungen an den Bedienelementen ohne Auto testen.

**Lokal testen am PC:** im Ordner `docs/` den Befehl `python -m http.server 8000` ausführen und `http://localhost:8000` in Chrome öffnen. Ein PC mit Bluetooth kann sich so auch mit einem Auto verbinden.

## 3. Bedienung

- **Auto suchen** → im Browser-Dialog das eigene `RoboCar-XX` wählen.
- LED am ESP32: **blinkt langsam** = wartet auf Verbindung, **leuchtet** = verbunden, **blinkt schnell** = „Identifizieren“ (⚙ → „LED blinken lassen“).
- Ist ein Auto verbunden, ist es für andere Handys **unsichtbar**. Fremde können es also nicht übernehmen.
- Fahrmodi (Taste oben): **Joystick** (ein Daumen), **2 Hände** (links Gas, rechts Lenken), **Panzer** (links/rechts je eine Seite).
- **BOOST** gedrückt halten → Boost-Geschwindigkeit statt Max.-Geschwindigkeit.
- Ein Bluetooth-Gamecontroller am Handy funktioniert ebenfalls (linker Stick, RT = Boost, A/B/X/Y = AUX 1–4).

### Einstellungen (⚙, werden im Auto gespeichert)

| Einstellung | Wirkung |
|---|---|
| Gruppe 1–16 | Name `RoboCar-XX` (Neustart des Autos, App verbindet automatisch neu) |
| Max. / Boost-Geschwindigkeit | Begrenzung der PWM in % |
| Lenk-Empfindlichkeit | wie stark die Lenkung wirkt |
| Trim | Geradeauslauf korrigieren |
| Motoren A/B falsch herum, Seiten vertauscht, Lenkung umkehren | Verkabelungsfehler per Software beheben statt umzustecken |

## 4. Sicherheit

- **Failsafe:** Kommt 300 ms kein Steuerpaket an, stoppen die Motoren sofort. Gründe dafür: Verbindung weg, App im Hintergrund, Handy gesperrt.
- Joystick loslassen → 0. Die App sendet beim Wechsel in den Hintergrund sofort Stopp.
- Bremsen und Richtungswechsel wirken sofort, nur das Beschleunigen ist gerampt (schont den LiPo).
- `onFailsafe()` in `aux_user.cpp`: Eigene Aktoren (Kicker etc.) gehen bei Verbindungsverlust in einen sicheren Zustand.

## 5. Eigene Erweiterungen (für die Studierenden)

| Wo | Was |
|---|---|
| `docs/controls.js` → `AUX_KANAELE` | Name und Art (Taster / Schalter / Schieber) der 4 AUX-Kanäle |
| `firmware/RoboCar_BLE/aux_user.cpp` → `onAux(kanal, wert)` | Was beim AUX-Wert passieren soll. Beispiele für einen Servo-Kicker (GPIO 4) und Licht (GPIO 16) sind enthalten und mit `#define … 1` aktivierbar |

Freie Pins: **GPIO 4, 16, 17, 18, 19, 21, 22, 23** (Eingänge zusätzlich 34, 35, 36, 39).
Nicht anfassen: `ble.js`, `ble_link.cpp`, `motor.cpp` (sonst passt das Protokoll nicht mehr).

## 6. Pinbelegung (geändert gegenüber WS25/26!)

| L298N | ESP32 | Bemerkung |
|---|---|---|
| IN1 | GPIO 14 | Motoren A |
| IN2 | GPIO 27 | Motoren A |
| ENA | **GPIO 33** | bisher 12, siehe unten |
| IN3 | GPIO 26 | Motoren B |
| IN4 | GPIO 25 | Motoren B |
| ENB | **GPIO 32** | bisher 13 |
| GND | GND | gemeinsame Masse ESP32 ↔ L298N |

Warum geändert: **GPIO 12 ist ein Strapping-Pin.** Liegt er beim Einschalten auf HIGH, stellt der ESP32 die Flash-Spannung falsch ein und startet nicht. Außerdem passten in WS25/26 die Pins in der Anleitung und im Programm nicht zueinander.
Welche Seite A oder B ist und in welche Richtung die Motoren drehen, ist egal. Das korrigieren die Studierenden in der App.

## 7. Protokoll (BLE GATT)

Service `6e8a1000-5c3b-4b6e-9a2f-0c1d2e3f4a50`

| Characteristic | UUID | Richtung | Inhalt |
|---|---|---|---|
| control | `6e8a1001-…` | App → Auto, Write without Response, 25 Hz | 9 Byte: `ver, seq, gas(int8 ±100), lenkung(int8 ±100), tasten(bit0 Boost, bit1 Identify), aux1..aux4 (0..255)` |
| config | `6e8a1002-…` | Read/Write | 8 Byte: `ver, gruppe(0..16), max%, boost%, trim(int8 ±50), flags(bit0 invA, bit1 invB, bit2 swap, bit3 invLenkung), lenk%, 0` |
| status | `6e8a1003-…` | Auto → App, Notify bei jedem 5. Paket | 6 Byte: `fwMajor, fwMinor, letzte seq, flags(bit0 Failsafe, bit1 Gruppe gesetzt), gruppe, 0` |

Protokollversion = 1. Die App misst die Latenz über das `seq`-Echo im Status (Anzeige oben, in ms).
Der Gerätename steht im Advertising, die Service-UUID in der Scan-Antwort.

## 8. Noch zu testen (vor der Projektwoche)

- [ ] Einzelauto auf dem Bock: Fahren, Trim/Invert, Einstellungen nach Neustart noch da
- [ ] Failsafe: App schließen / Bluetooth aus / außer Reichweite → Stopp < 300 ms
- [ ] Zweites Handy sieht verbundenes Auto nicht
- [ ] Latenzanzeige < 50 ms
- [ ] 2 Android-Handys (Chrome) + 1 iPhone (Bluefy)
- [ ] 8 Autos + 8 Handys gleichzeitig, 10 min Spiel
- [ ] Boot-Test 20× Aus/Ein mit neuer Pinbelegung
