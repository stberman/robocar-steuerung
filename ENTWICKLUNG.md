# Entwicklungsnotizen RoboCar-Steuerung (WS26/27)

Kontext für alle, die später an der Steuerung weiterarbeiten. Hier steht, **warum** etwas so gebaut ist und was noch offen ist.
Bedienung, Flashen und Protokoll stehen in der [README.md](README.md).

Stand: 10.11.2026 · Firmware 1.1 · App-Cache `robocar-v4`

---

## 1. Ausgangslage und Ziel

- **Projektwoche:** Erstsemester Maschinenbau, TH Köln. 16 Gruppen bauen ein Roboterauto aus einem Kit (ESP32-WROOM-32 DevKit, L298N, 4 Motoren, JoyIt-4x4-V3-Chassis) und spielen danach Autofußball 4 gegen 4.
- **Bisher (WS25/26):** Raspberry Pi als WLAN-Access-Point mit MQTT-Broker, USB-Gamepads und Web-UI zur Zuordnung (`WS25_26/Program/Auto_soccer/`). Aufbau und Wartung waren aufwendig.
- **Neu (WS26/27):** Jede Gruppe steuert ihr Auto **direkt mit dem eigenen Handy per Bluetooth LE**.

## 2. Entscheidungen der Veranstalter

| Frage | Entscheidung |
|---|---|
| Welche Handys? | eigene Handys der Studierenden, Android **und** iPhone gemischt |
| App änderbar? | ja, die Studierenden sollen die App selbst anpassen können |
| Zentrale Spielleitung (Start/Stopp für alle)? | nein, nur Failsafe im Auto |
| Zusatzkanäle? | ja, 4 AUX-Kanäle für eigene Erweiterungen (Kicker, Licht …) |
| Hosting | GitHub, Account `stberman`, öffentliches Repo `robocar-steuerung` |
| Ablage | alle neuen Unterlagen in `WS26_27/` |
| Autoname | wählbar über `#define AUTO_NAME` im Kopf der .ino. `RoboCar-` wird **immer** davorgesetzt, die App filtert nur `RoboCar-*` |
| Kommentare | Firmware ausführlich für Programmier-Anfänger kommentiert. Einstellbare Zeilen sichtbar lassen, nicht auskommentieren |

## 3. Technische Entscheidungen und Begründung

- **BLE statt Bluetooth Classic:** iPhones unterstützen kein Classic SPP.
- **Web-App (PWA) mit Web Bluetooth statt nativer App:** Ein Code für alle Plattformen. Ändern geht mit Texteditor und Browser, ohne App Store und ohne Android Studio.
  - Android: Chrome.
  - iPhone: Browser **„Bluefy“**, weil Safari kein Web Bluetooth kann.
  - Fallback, falls Bluefy beim Test durchfällt: Flutter-App mit demselben Protokoll.
- **NimBLE-Arduino 2.x** statt des Standard-BLE-Stacks: kleiner und stabiler.
- **Eine Firmware für alle Autos:** Gruppe, Geschwindigkeit, Trim und Kalibrierung werden in der App eingestellt und im NVS (`Preferences`) gespeichert.
- **Nur eine Verbindung pro Auto:** Ist ein Handy verbunden, stoppt das Advertising. Kein anderes Handy kann das Auto übernehmen.
- **Failsafe 300 ms:** Kommt kein Steuerpaket, stehen die Motoren. Die App sendet 25 Pakete pro Sekunde.
- **Pinbelegung geändert:** ENA 12 → 33 und ENB 13 → 32. GPIO 12 ist ein Strapping-Pin; liegt er beim Booten auf HIGH, startet der ESP32 nicht. In WS25/26 passten außerdem die Pins in Anleitung und Code nicht zueinander.
- **Advertising:** Der Name steht im Advertising-Paket, die Service-UUID in der Scan-Antwort. Beides zusammen passt nicht in 31 Byte. Daher ist der eigene Name auf 18 Zeichen begrenzt (+ `RoboCar-`).

## 4. Fahrlogik (motor.cpp), Verlauf der Korrekturen

1. **Panzer-Modus:** Die nicht angesteuerte Seite lief leicht mit.
   - Ursache: Die App kodiert L/R als Gas = (L+R)/2 und Lenkung = (L−R)/2, und die Lenk-Empfindlichkeit (70 %) wurde zusätzlich angewendet. Die Anlaufschwelle hat den Restwert dann auf PWM ≥ 90 angehoben.
   - Lösung: Tasten-Bit `BTN_PANZER` (0x04). Im Panzer-Modus wirkt die Lenk-Empfindlichkeit nicht, und der Totbereich gilt für jede Seite einzeln.
2. **Leichte Kurven waren kaum möglich:**
   - Ursache: Mit 4 festen Rädern (Panzerlenkung) muss die innere Seite deutlich langsamer laufen. Die alte Formel L = t+s, R = t−s erzeugte nur kleine Unterschiede, und die Anlaufschwelle hat sie weiter gestaucht.
   - Neue Formel:
     - äußere Seite = t
     - innere Seite = t · max(−1, 1 − |s|/Kurvenschärfe)
     - Drehanteil s · (1 − |t|) für Drehen auf der Stelle
     - Welche Seite innen ist, hängt von der Lenkrichtung **und** der Fahrtrichtung ab (rückwärts geprüft).
3. **Kalibrierung in der App** (Reiter „Kalibrieren“, ab Firmware 1.1):
   - Werte: Anlaufschwelle, Trim, Kurvenschärfe, Expo, Totbereich, Anfahrrampe.
   - Wirkt sofort und wird gespeichert.
   - Test-Tasten fahren das Auto, solange sie gedrückt sind (`Controls.test` in controls.js).

## 5. Kompatibilität

- **Konfigurationspaket:** 16 statt 8 Byte.
  - Die Firmware akzeptiert weiterhin 8 Byte (alte App); die Kalibrierung bleibt dann unverändert.
  - Die App erkennt alte Firmware an der Paketlänge 8 und sperrt die Kalibrier-Regler.
- **NVS:** Neue Felder stehen hinten im `struct Settings`. Ein kürzerer gespeicherter Datensatz wird übernommen. Die Gruppe bleibt beim Firmware-Update also erhalten.
- **Neue Felder** immer **hinten** anhängen, sonst passen alte gespeicherte Daten nicht mehr.

## 6. Arbeitsweise und Werkzeuge

- **Repo:** https://github.com/stberman/robocar-steuerung, lokaler Klon `C:\Users\stber\Projects\robocar-steuerung`. Das Repo ist die führende Version.
  - Die App liegt in `docs/` und wird über GitHub Pages ausgeliefert: https://stberman.github.io/robocar-steuerung/ (Demo: `?demo`).
- **Ablage-Kopie:** `WS26_27/Steuerung/` im Teams-/OneDrive-Ordner. Dort heißt `docs/` → `app/`. Nach Änderungen im Repo die Dateien dorthin kopieren.
- **App-Updates:** `CACHE` in `docs/sw.js` hochzählen, sonst behalten installierte Apps die alte Version.
- **Kompilieren ohne IDE:**
  ```
  "C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe" compile --config-file C:\Users\stber\.arduinoIDE\arduino-cli.yaml -b esp32:esp32:esp32 --warnings default firmware\RoboCar_BLE
  ```
  - Getestet mit esp32-Core 3.3.12 und NimBLE-Arduino 2.5.2. Ergebnis: ca. 605 kB (46 %), ohne Warnungen.
- **App ohne Handy testen:** Headless Chrome mit `index.html?demo` (Screenshot bzw. `--dump-dom`).
  - `requestAnimationFrame` läuft dort nicht; im Test `Controls._frame()` von Hand aufrufen.
- **Fehler „Preferences.h: No such file or directory“:**
  - Ursache 1: falsches Board gewählt. Jetzt fängt `config.h` das mit `#error "Falsches Board!"` ab.
  - Ursache 2: veralteter Build-Cache der IDE nach einem Boardwechsel. Lösung: IDE schließen und `%LOCALAPPDATA%\arduino\sketches\<hash>` löschen.

## 7. Weitere Ergebnisse dieser Arbeit

- **Aufbauanleitung v4:** `WS26_27/Anleitung/Aufbauanleitung für das Ferngesteuerte Auto v4 (WS26-27).docx` und `.pdf`.
  - Neues Kapitel 7 „Steuerung per Handy-App“.
  - Kapitel 5.2 mit der neuen Pinbelegung.
  - Abb. 8 durch den neuen Verkabelungsplan ersetzt. Quelle: `WS26_27/Anleitung/Verkabelungsplan/make_wiring.py` → SVG/PNG.
- **Aushang mit QR-Code:** `WS26_27/Steuerung/Aushang_RoboCar_App_QR.pdf`.

## 8. Offene Punkte

- [ ] **Hardwaretests:** Checkliste in README Abschnitt 8 (Failsafe, 8 Autos gleichzeitig, Android + iPhone/Bluefy, Boot-Test). Alles bisher nur kompiliert und im Demo-Modus geprüft, **nicht** am echten Auto.
- [ ] **Anleitung nachziehen:** Anleitung v4 und `Anleitung_Kapitel_Steuerung.md` kennen den Reiter „Kalibrieren“ und `AUTO_NAME` noch nicht. Trim steht dort noch unter ⚙ allgemein.
- [ ] **Deckblatt der Anleitung:** steht noch auf „Ort, 26.11.2023“.
- [ ] **Motoren und Treiber:**
  - Gemessen: bei 100 % ca. **9,6 V** an den Motoren. Der L298N verliert je nach Strom ca. 2–4 V.
  - Motortyp und Nennspannung sind nicht dokumentiert (keine TT-Gelbmotoren), ebenso die Akkuspannung am L298N.
  - Erst wenn beides bekannt ist, entscheiden, ob ein MOSFET-Treiber (TB6612FNG/BTS7960) sinnvoll ist. Die Firmware-Logik (IN/IN/PWM) würde passen.
- [ ] **Firmware auf alle Autos flashen:** Panzer-Fix, Kurvenlogik und Kalibrierung brauchen Firmware ≥ 1.1.
