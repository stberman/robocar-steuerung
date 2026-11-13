# Beispiele für eigene Erweiterungen

Diese Dateien gehören zum Kapitel „Projekt: Servo-Kicker“ der Anleitung für Studierende.
Jede Datei ist eine **komplette** Version von `aux_user.cpp`.

| Datei | Was macht sie? |
|---|---|
| `kicker_automatik.cpp` | Ein kurzer Druck auf AUX 1 = ein Schuss. Der Servo fährt aus und nach 250 ms von selbst wieder ein. |
| `servo_schieber.cpp` | Der Schieber auf AUX 3 stellt den Servo-Winkel stufenlos ein (z. B. für einen Greifarm). |

## So benutzen Sie ein Beispiel

1. In der Arduino IDE `firmware/RoboCar_BLE/RoboCar_BLE.ino` öffnen.
2. Den Reiter `aux_user.cpp` anklicken.
3. Den **gesamten** Inhalt markieren (Strg+A) und löschen.
4. Den Inhalt der Beispieldatei hier einfügen und hochladen.

**Wichtig:** Die Beispieldateien bleiben in diesem Ordner. Kopieren Sie sie **nicht** als eigene Datei
in den Ordner `firmware/RoboCar_BLE/`. Die Arduino IDE übersetzt alle `.cpp`-Dateien in diesem Ordner,
und dann gäbe es jede Funktion doppelt.
