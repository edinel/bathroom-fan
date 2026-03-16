// Bathroom Fan Controller - Physical Button + LED
//
// Hardware: Adafruit #481 Rugged Metal Pushbutton with Blue LED Ring (16mm)
//
// Wiring:
//   Button COM  → GND
//   Button NO   → BUTTON_PIN (uses INPUT_PULLUP, no external resistor needed)
//   LED +       → 5V rail (from buck converter)
//   LED −       → NPN transistor collector (e.g. 2N3904)
//   Transistor base → 1kΩ resistor → LED_PIN (GPIO)
//   Transistor emitter → GND
//
//   Note: LED is rated 6V with built-in resistor; at 5V it will be slightly dim.
//   For full brightness: power LED from 12V rail with a 470Ω series resistor
//   in place of the built-in resistor path (check button datasheet).
//
// Behavior:
//   - Press toggles manual override on/off
//   - Entering manual: forces fan to 100%
//   - Returning to auto: resumes humidity-based control
//   - LED is ON whenever fan duty cycle > 0 (mirrors actual fan state)
//   - Web server override and button override share the same g_manualOverride flag;
//     either one can cancel the other.
//
// Integration:
//   1. Call setupButton() from setup()
//   2. Call handleButton() from loop() on every iteration

#include <Arduino.h>

#define BUTTON_PIN  15    // GPIO connected to button NO terminal
#define LED_PIN     14    // GPIO driving NPN transistor base

static const unsigned long DEBOUNCE_MS = 50;
static unsigned long lastPressTime  = 0;
static bool          lastButtonState = HIGH;  // HIGH = not pressed (INPUT_PULLUP)

// Shared state — owned by main.cpp
extern int  g_dutyCycle;
extern bool g_manualOverride;
extern int  g_manualDuty;

void setupButton() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
}

void handleButton() {
  bool currentState = digitalRead(BUTTON_PIN);
  unsigned long now = millis();

  // Detect falling edge (press) with debounce
  if (lastButtonState == HIGH && currentState == LOW) {
    if (now - lastPressTime > DEBOUNCE_MS) {
      lastPressTime = now;

      if (g_manualOverride) {
        g_manualOverride = false;
        Serial.println("Button: auto mode");
      } else {
        g_manualOverride = true;
        g_manualDuty     = 100;
        Serial.println("Button: manual override ON (100%)");
      }
    }
  }
  lastButtonState = currentState;

  // LED mirrors fan running state
  digitalWrite(LED_PIN, g_dutyCycle > 0 ? HIGH : LOW);
}
