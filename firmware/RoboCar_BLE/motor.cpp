/*
 * motor.cpp – Motoren ansteuern
 *
 * Das Auto lenkt wie ein Panzer: Es hat keine lenkbaren Räder. Zum Lenken
 * dreht eine Seite schneller als die andere.
 *
 * Ablauf:
 *   1. motorSetTarget(): Aus Gas und Lenkung ausrechnen, wie schnell die
 *      linke und die rechte Seite drehen sollen ("Sollwert").
 *   2. motorUpdate(): alle 10 ms die tatsächlichen Werte ("Istwert")
 *      schrittweise an den Sollwert annähern (Anfahrrampe).
 *   3. applyOutputs()/driveChannel(): die Werte an die Pins des L298N geben.
 *
 * Alle Geschwindigkeiten sind PWM-Werte von -255 bis 255:
 *   positiv = vorwärts, negativ = rückwärts, 0 = Stillstand.
 */
#include "motor.h"
#include "config.h"
#include "settings.h"

// Sollwerte (wohin die Motoren wollen) und Istwerte (was gerade anliegt)
static int targetLeft = 0, targetRight = 0;
static int currentLeft = 0, currentRight = 0;

// -------------------------------------------------------------------
// Eine Seite (= einen Ausgang des L298N) ansteuern.
// Der L298N braucht pro Motorausgang drei Signale:
//   in1/in2: Drehrichtung   HIGH/LOW = vorwärts, LOW/HIGH = rückwärts,
//                           LOW/LOW  = Motor aus
//   en:      Geschwindigkeit als PWM (0..255)
// -------------------------------------------------------------------
static void driveChannel(int in1, int in2, int en, int pwm) {
  if (pwm > 0) {
    digitalWrite(in1, HIGH);
    digitalWrite(in2, LOW);
  } else if (pwm < 0) {
    digitalWrite(in1, LOW);
    digitalWrite(in2, HIGH);
  } else {
    digitalWrite(in1, LOW);
    digitalWrite(in2, LOW);
  }
  ledcWrite(en, abs(pwm));   // abs() = Betrag, also ohne Vorzeichen
}

// -------------------------------------------------------------------
// Linke/rechte Fahrzeugseite auf die L298N-Ausgänge A/B verteilen.
// Hier wirken die Häkchen aus der App ("Motoren A drehen falsch herum",
// "Links und rechts vertauscht"). So muss niemand Kabel umstecken.
// -------------------------------------------------------------------
static void applyOutputs() {
  int a = currentLeft, b = currentRight;
  // "settings.flags & CFG_SWAP_SIDES" prüft, ob dieses Häkchen gesetzt ist
  if (settings.flags & CFG_SWAP_SIDES) { int t = a; a = b; b = t; }   // Seiten tauschen
  if (settings.flags & CFG_INVERT_A) a = -a;                          // Richtung umdrehen
  if (settings.flags & CFG_INVERT_B) b = -b;
  driveChannel(PIN_IN1, PIN_IN2, PIN_ENA, a);
  driveChannel(PIN_IN3, PIN_IN4, PIN_ENB, b);
}

void motorBegin() {
  pinMode(PIN_IN1, OUTPUT);
  pinMode(PIN_IN2, OUTPUT);
  pinMode(PIN_IN3, OUTPUT);
  pinMode(PIN_IN4, OUTPUT);
  // ledcAttach: den Pin als PWM-Ausgang einrichten (Frequenz, Auflösung)
  ledcAttach(PIN_ENA, PWM_FREQ_HZ, PWM_RESOLUTION);
  ledcAttach(PIN_ENB, PWM_FREQ_HZ, PWM_RESOLUTION);
  motorStopNow();
}

// Anlaufschwelle aus der Kalibrierung von % in einen PWM-Wert umrechnen
static int minStartPwm() { return settings.minStart * 255 / 100; }

// -------------------------------------------------------------------
// Einen Wert von -1.0 bis 1.0 in PWM (-255..255) umrechnen.
// Getriebemotoren drehen bei kleinem PWM gar nicht an (sie brummen nur).
// Deshalb beginnt die Skala bei der Anlaufschwelle:
//   Wert 0             -> PWM 0 (aus)
//   Wert knapp über 0  -> PWM = Anlaufschwelle (Motor dreht gerade so)
//   Wert 1             -> PWM 255 (volle Kraft)
// "scale" begrenzt zusätzlich (Max.-Geschwindigkeit und Trim).
// -------------------------------------------------------------------
static int toPwm(float v, float scale) {
  if (fabsf(v) < 0.001f) return 0;                    // fabsf() = Betrag einer Kommazahl
  int minPwm = minStartPwm();
  float mag = fabsf(v) * scale;                       // 0..1
  int pwm = minPwm + (int)(mag * (255 - minPwm));
  pwm = constrain(pwm, 0, 255);                       // constrain() = auf 0..255 begrenzen
  return v > 0 ? pwm : -pwm;                          // Vorzeichen (Richtung) wieder dranhängen
}

// -------------------------------------------------------------------
// Expo: kleine Joystick-Ausschläge werden schwächer, Vollausschlag
// bleibt 100 %. Damit kann man feiner steuern.
// Rechnung: Mischung aus linear (v) und kubisch (v*v*v).
// -------------------------------------------------------------------
static float expo(float v) {
  float e = settings.expo / 100.0f;
  return (1.0f - e) * v + e * v * v * v;
}

// -------------------------------------------------------------------
// Aus Gas und Lenkung die Sollwerte für links und rechts berechnen.
// -------------------------------------------------------------------
void motorSetTarget(int8_t throttle, int8_t steer, bool boost, bool panzer) {
  const float dz = settings.deadzone / 100.0f;        // Totbereich als 0..1
  if (settings.flags & CFG_INVERT_STEER) steer = -steer;

  // In Kommazahlen von -1.0 bis 1.0 umrechnen ("f" = float, Kommazahl)
  float t = throttle / 100.0f;
  float s = steer / 100.0f;
  float left, right;

  if (panzer) {
    // --- Panzer-Modus: linker Regler = linke Seite, rechter = rechte Seite.
    // Die App verschickt das als Gas = (L+R)/2 und Lenkung = (L-R)/2.
    // Gas + Lenkung bzw. Gas - Lenkung ergibt wieder genau L und R.
    // Die Lenk-Empfindlichkeit wirkt hier bewusst NICHT, sonst würde die
    // nicht angesteuerte Seite mitlaufen.
    left = t + s;
    right = t - s;
    // Totbereich für jede Seite einzeln (auch Rundungsreste ignorieren)
    left  = fabsf(left)  < dz ? 0 : expo(constrain(left, -1.0f, 1.0f));
    right = fabsf(right) < dz ? 0 : expo(constrain(right, -1.0f, 1.0f));
  } else {
    // --- Joystick und 2 Hände
    // Kleine Ausschläge (Totbereich) ignorieren, damit das Auto nicht
    // kriecht, wenn der Daumen den Joystick nur leicht berührt.
    if (fabsf(t) < dz) t = 0;
    if (fabsf(s) < dz) s = 0;
    t = expo(t);
    s = expo(s) * (settings.steerRate / 100.0f);       // Lenk-Empfindlichkeit aus der App

    // Kurvenfahrt: Mit 4 festen Rädern dreht das Auto erst, wenn die
    // kurveninnere Seite DEUTLICH langsamer läuft, weil die Räder dabei
    // seitlich über den Boden rutschen müssen. Darum:
    //   äußere Seite = Gas
    //   innere Seite = Gas, aber abgebremst, je weiter man lenkt.
    // "Kurvenschärfe" (curveFull) = Lenkausschlag, bei dem die innere Seite
    // steht. Beim doppelten Ausschlag dreht sie mit voller Kraft rückwärts.
    // Beispiel Kurvenschärfe 50 %: Lenkung 25 % -> innen halbe Geschwindigkeit,
    //                              Lenkung 50 % -> innen steht.
    float cf = settings.curveFull / 100.0f;
    float a = fabsf(s);                                 // wie weit gelenkt wird, ohne Richtung
    float outer = t;
    float inner = t * max(-1.0f, 1.0f - a / cf);

    // Ohne Gas soll das Auto auf der Stelle drehen (eine Seite vor, eine
    // zurück). Dieser Anteil wird kleiner, je mehr Gas man gibt.
    float pivot = s * (1.0f - fabsf(t));

    // Welche Seite innen ist, hängt von der Lenkrichtung UND der
    // Fahrtrichtung (vor/zurück) ab.
    bool rechtsInnen = (s >= 0) == (t >= 0);
    if (rechtsInnen) { left = outer + pivot; right = inner - pivot; }
    else             { left = inner + pivot; right = outer - pivot; }
  }

  // Ist ein Wert größer als 1 geworden, beide Seiten gleichmäßig
  // verkleinern ("normieren"). Das Verhältnis links/rechts bleibt so
  // erhalten und das Auto lenkt auch bei Vollgas noch.
  float m = max(fabsf(left), fabsf(right));
  if (m > 1.0f) { left /= m; right /= m; }

  // Geschwindigkeitsbegrenzung aus der App (normal oder mit Boost)
  float speed = (boost ? settings.boostSpeed : settings.maxSpeed) / 100.0f;

  // Trimmung (Geradeauslauf): positive Werte schwächen die rechte Seite,
  // negative die linke. max(0, x) = x, aber nie kleiner als 0.
  float scaleL = speed * (1.0f - max(0, -settings.trim) / 100.0f);
  float scaleR = speed * (1.0f - max(0, (int)settings.trim) / 100.0f);

  targetLeft = toPwm(left, scaleL);
  targetRight = toPwm(right, scaleR);
}

// -------------------------------------------------------------------
// Einen Schritt vom Istwert (cur) Richtung Sollwert (target) gehen.
// Die Anfahrrampe gilt nur fürs Beschleunigen, damit der Akku nicht durch
// einen hohen Anlaufstrom überlastet wird. Bremsen geht sofort, damit das
// Auto direkt reagiert, wenn man den Joystick loslässt.
// -------------------------------------------------------------------
static int stepTowards(int cur, int target) {
  // Anhalten oder Richtungswechsel: erst sofort auf 0
  if (cur != 0 && (target == 0 || (target > 0) != (cur > 0))) return 0;
  // Langsamer werden: sofort
  if (abs(target) <= abs(cur)) return target;
  // Rampe in der App ausgeschaltet: sofort auf den Sollwert
  if (settings.ramp == 0) return target;
  // Beschleunigen in Schritten. Beim Anfahren gleich ab der Anlaufschwelle,
  // denn darunter dreht der Motor sowieso nicht.
  int step = (255 + settings.ramp - 1) / settings.ramp;   // PWM-Zuwachs pro 10 ms
  int dir = target > 0 ? 1 : -1;                          // Richtung merken
  int mag = max(abs(cur) + step, minStartPwm());
  return dir * min(mag, abs(target));                     // nicht über das Ziel hinaus
}

void motorUpdate() {
  int nl = stepTowards(currentLeft, targetLeft);
  int nr = stepTowards(currentRight, targetRight);
  // Nur wenn sich etwas geändert hat, die Pins neu setzen
  if (nl != currentLeft || nr != currentRight) {
    currentLeft = nl;
    currentRight = nr;
    applyOutputs();
  }
}

void motorStopNow() {
  targetLeft = targetRight = 0;
  currentLeft = currentRight = 0;
  applyOutputs();
}
