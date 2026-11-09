#include "motor.h"
#include "config.h"
#include "settings.h"

// Soll- und Istwerte als vorzeichenbehaftete PWM (-255..255)
static int targetLeft = 0, targetRight = 0;
static int currentLeft = 0, currentRight = 0;

// Eine Seite (= ein Ausgang des L298N) ansteuern
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
  ledcWrite(en, abs(pwm));
}

// Linke/rechte Fahrzeugseite auf Kanal A/B abbilden (Seiten tauschen, Richtung umkehren)
static void applyOutputs() {
  int a = currentLeft, b = currentRight;
  if (settings.flags & CFG_SWAP_SIDES) { int t = a; a = b; b = t; }
  if (settings.flags & CFG_INVERT_A) a = -a;
  if (settings.flags & CFG_INVERT_B) b = -b;
  driveChannel(PIN_IN1, PIN_IN2, PIN_ENA, a);
  driveChannel(PIN_IN3, PIN_IN4, PIN_ENB, b);
}

void motorBegin() {
  pinMode(PIN_IN1, OUTPUT);
  pinMode(PIN_IN2, OUTPUT);
  pinMode(PIN_IN3, OUTPUT);
  pinMode(PIN_IN4, OUTPUT);
  ledcAttach(PIN_ENA, PWM_FREQ_HZ, PWM_RESOLUTION);
  ledcAttach(PIN_ENB, PWM_FREQ_HZ, PWM_RESOLUTION);
  motorStopNow();
}

// Anlaufschwelle als PWM-Wert (aus der Kalibrierung)
static int minStartPwm() { return settings.minStart * 255 / 100; }

// Wert -1..1 in PWM umrechnen; kleine Werte auf die Anlaufschwelle anheben
static int toPwm(float v, float scale) {
  if (fabsf(v) < 0.001f) return 0;
  int minPwm = minStartPwm();
  float mag = fabsf(v) * scale;                       // 0..1
  int pwm = minPwm + (int)(mag * (255 - minPwm));
  pwm = constrain(pwm, 0, 255);
  return v > 0 ? pwm : -pwm;
}

// Expo: kleine Ausschlaege feinfuehliger, Vollausschlag bleibt 100 %
static float expo(float v) {
  float e = settings.expo / 100.0f;
  return (1.0f - e) * v + e * v * v * v;
}

void motorSetTarget(int8_t throttle, int8_t steer, bool boost, bool panzer) {
  const float dz = settings.deadzone / 100.0f;
  if (settings.flags & CFG_INVERT_STEER) steer = -steer;

  float t = throttle / 100.0f;
  float s = steer / 100.0f;
  float left, right;

  if (panzer) {
    // Panzer-Modus: Die App kodiert linke/rechte Seite als Gas/Lenkung.
    // links = Gas + Lenkung, rechts = Gas - Lenkung ergibt genau die beiden
    // Seiten. Die Lenk-Empfindlichkeit wirkt hier nicht, sonst laeuft die
    // nicht angesteuerte Seite mit.
    left = t + s;
    right = t - s;
    // Deadzone je Seite (auch Rundungsreste aus der App ignorieren)
    left  = fabsf(left)  < dz ? 0 : expo(constrain(left, -1.0f, 1.0f));
    right = fabsf(right) < dz ? 0 : expo(constrain(right, -1.0f, 1.0f));
  } else {
    if (fabsf(t) < dz) t = 0;
    if (fabsf(s) < dz) s = 0;
    t = expo(t);
    s = expo(s) * (settings.steerRate / 100.0f);

    // Kurvenfahrt: Ein Auto mit Panzerlenkung (4 feste Raeder) dreht erst,
    // wenn die innere Seite DEUTLICH langsamer laeuft, weil die Raeder seitlich
    // rutschen muessen. Daher: aussen = Gas, innen wird abgebremst.
    // "Kurve" (curveFull) = Lenkausschlag, bei dem die innere Seite steht.
    // Beim doppelten Ausschlag dreht sie mit voller Geschwindigkeit rueckwaerts.
    float cf = settings.curveFull / 100.0f;
    float a = fabsf(s);
    float outer = t;
    float inner = t * max(-1.0f, 1.0f - a / cf);
    // Ohne Gas auf der Stelle drehen; dieser Anteil wird mit mehr Gas kleiner.
    float pivot = s * (1.0f - fabsf(t));
    // Welche Seite innen ist, haengt von Lenkrichtung UND Fahrtrichtung ab
    // (gleiche Logik wie beim Drehen auf der Stelle: links + Lenkung).
    bool rechtsInnen = (s >= 0) == (t >= 0);
    if (rechtsInnen) { left = outer + pivot; right = inner - pivot; }
    else             { left = inner + pivot; right = outer - pivot; }
  }

  // Normieren statt abschneiden: Das Verhaeltnis links/rechts bleibt erhalten,
  // dadurch lenkt das Auto auch bei Vollgas noch.
  float m = max(fabsf(left), fabsf(right));
  if (m > 1.0f) { left /= m; right /= m; }

  // Geschwindigkeitsbegrenzung (normal / Boost)
  float speed = (boost ? settings.boostSpeed : settings.maxSpeed) / 100.0f;

  // Trimmung: positive Werte schwaechen die rechte Seite, negative die linke
  float scaleL = speed * (1.0f - max(0, -settings.trim) / 100.0f);
  float scaleR = speed * (1.0f - max(0, (int)settings.trim) / 100.0f);

  targetLeft = toPwm(left, scaleL);
  targetRight = toPwm(right, scaleR);
}

// Die Rampe gilt nur fuer das Beschleunigen. Bremsen geht sofort, damit das
// Auto direkt reagiert, wenn man den Joystick loslaesst.
static int stepTowards(int cur, int target) {
  // Stopp oder Richtungswechsel: erst sofort auf 0
  if (cur != 0 && (target == 0 || (target > 0) != (cur > 0))) return 0;
  // Langsamer werden: sofort
  if (abs(target) <= abs(cur)) return target;
  // Rampe aus: sofort auf den Sollwert
  if (settings.ramp == 0) return target;
  // Beschleunigen: in Schritten, beim Anfahren direkt ab der Anlaufschwelle
  int step = (255 + settings.ramp - 1) / settings.ramp;   // PWM pro 10 ms
  int dir = target > 0 ? 1 : -1;
  int mag = max(abs(cur) + step, minStartPwm());
  return dir * min(mag, abs(target));
}

void motorUpdate() {
  int nl = stepTowards(currentLeft, targetLeft);
  int nr = stepTowards(currentRight, targetRight);
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
