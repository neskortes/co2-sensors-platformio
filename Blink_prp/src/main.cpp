#include <Arduino.h>

// --- Pins ---
#ifndef LED_BUILTIN
#define LED_BUILTIN 2          // internal LED (algengt á ESP32-WROOM-32 dev boards)
#endif
const int EXT_LED_PIN = 5;     // external LED á GPIO5

// --- Modes ---
#define MODE_SOS        1
#define MODE_BREATH     2
#define MODE_HEARTBEAT  3

#define ACTIVE_MODE MODE_BREATH   // <-- breyttu þessu til að skipta um forrit

// --- PWM settings (for BREATH) ---
const int freq = 5000;
const int resolution = 8;      // 0..255
const int chInt = 0;           // PWM channel for internal LED
const int chExt = 1;           // PWM channel for external LED

// Set to true if external LED is wired "active-low" (inverted brightness)
const bool INVERT_EXT_LED = false;

// ---------------- SOS (uses digital on LED_BUILTIN) ----------------
void dot()  { digitalWrite(LED_BUILTIN, HIGH); delay(200); digitalWrite(LED_BUILTIN, LOW); delay(200); }
void dash() { digitalWrite(LED_BUILTIN, HIGH); delay(600); digitalWrite(LED_BUILTIN, LOW); delay(200); }

void runSOS() {
  // S: ...
  dot(); dot(); dot();
  delay(400);
  // O: ---
  dash(); dash(); dash();
  delay(400);
  // S: ...
  dot(); dot(); dot();

  delay(1200);
}

// ---------------- HEARTBEAT (uses digital on LED_BUILTIN) ----------------
void pulse(int onMs, int offMs) {
  digitalWrite(LED_BUILTIN, HIGH); delay(onMs);
  digitalWrite(LED_BUILTIN, LOW);  delay(offMs);
}

void runHeartbeat() {
  pulse(80, 120);   // lub
  pulse(80, 600);   // dub + pause
}

// ---------------- BREATH (PWM on BOTH internal + external) ----------------
void setupBreath() {
  ledcSetup(chInt, freq, resolution);
  ledcAttachPin(LED_BUILTIN, chInt);

  ledcSetup(chExt, freq, resolution);
  ledcAttachPin(EXT_LED_PIN, chExt);
}

void runBreath() {
  for (int duty = 0; duty <= 255; duty++) {
    ledcWrite(chInt, duty);
    ledcWrite(chExt, INVERT_EXT_LED ? (255 - duty) : duty);
    delay(5);
  }
  for (int duty = 255; duty >= 0; duty--) {
    ledcWrite(chInt, duty);
    ledcWrite(chExt, INVERT_EXT_LED ? (255 - duty) : duty);
    delay(5);
  }
}

// ---------------- Arduino entry points ----------------
void setup() {
  Serial.begin(115200);
  Serial.println("Starting...");

#if ACTIVE_MODE == MODE_BREATH
  setupBreath();
#else
  pinMode(LED_BUILTIN, OUTPUT);
#endif

#if ACTIVE_MODE == MODE_SOS
  Serial.println("Mode: SOS");
#elif ACTIVE_MODE == MODE_BREATH
  Serial.println("Mode: BREATH (internal + external)");
#elif ACTIVE_MODE == MODE_HEARTBEAT
  Serial.println("Mode: HEARTBEAT");
#else
  Serial.println("Mode: UNKNOWN");
#endif
}

void loop() {
#if ACTIVE_MODE == MODE_SOS
  runSOS();
#elif ACTIVE_MODE == MODE_BREATH
  runBreath();
#elif ACTIVE_MODE == MODE_HEARTBEAT
  runHeartbeat();
#else
  delay(1000);
#endif
}
