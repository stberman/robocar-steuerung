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

// Wert -1..1 in PWM umrechnen; kleine Werte auf die Anlaufschwelle anheben
static int toPwm(float v, float scale) {
  if (fabsf(v) < 0.001f) return 0;
  float mag = fabsf(v) * scale;                       // 0..1
  int pwm = PWM_MIN_START + (int)(mag * (255 - PWM_MIN_START));
  pwm = constrain(pwm, 0, 255);
  return v > 0 ? pwm : -pwm;
}

void motorSetTarget(int8_t throttle, int8_t steer, bool boost, bool panzer) {
  if (abs(throttle) < DEADZONE_PCT) throttle = 0;
  if (abs(steer) < DEADZONE_PCT) steer = 0;
  if (settings.flags & CFG_INVERT_STEER) steer = -steer;

  float t = throttle / 100.0f;
  // Im Panzer-Modus kodiert die App linke/rechte Seite als Gas/Lenkung.
  // Die Lenk-Empfindlichkeit darf dort nicht wirken, sonst laeuft die
  // nicht angesteuerte Seite mit.
  float s = steer / 100.0f;
  if (!panzer) s *= settings.steerRate / 100.0f;

  float left, right;
  if (panzer) {
    // Panzer-Modus: links = Gas + Lenkung, rechts = Gas - Lenkung ergibt
    // genau die beiden Seiten aus der App.
    left = t + s;
    right = t - s;
    // Deadzone je Seite (Rundungsreste aus der App ignorieren)
    if (fabsf(left) < DEADZONE_PCT / 100.0f) left = 0;
    if (fabsf(right) < DEADZONE_PCT / 100.0f) right = 0;
  } else {
    // Kurvenfahrt: Ein Auto mit Panzerlenkung (4 feste Raeder) dreht erst,
    // wenn die innere Seite DEUTLICH langsamer laeuft, weil die Raeder seitlich
    // rutschen muessen. Daher: aussen = Gas, innen = Gas * (1 - 2*|Lenkung|).
    //   Lenkung 25 % -> innen halbe Geschwindigkeit
    //   Lenkung 50 % -> innen steht
    //   Lenkung 100 % -> innen rueckwaerts (dreht auf der Stelle)
    float a = fabsf(s);
    float outer = t;
    float inner = t * (1.0f - 2.0f * a);
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
  // Beschleunigen: in Schritten, beim Anfahren direkt ab der Anlaufschwelle
  int dir = target > 0 ? 1 : -1;
  int mag = max(abs(cur) + SLEW_PER_STEP, PWM_MIN_START);
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
