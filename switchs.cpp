#include "switchs.h"
#include "Config.h"

static bool isPressing = false;
static unsigned long pressStartTime = 0;
static uint8_t activePin = 0;

void setupSwitchs() {
  pinMode(BTN_POWER_PIN, OUTPUT);
  pinMode(BTN_UP_PIN, OUTPUT);
  pinMode(BTN_DOWN_PIN, OUTPUT);

  // וידוא שכל הטרנזיסטורים כבויים באתחול
  digitalWrite(BTN_POWER_PIN, LOW);
  digitalWrite(BTN_UP_PIN, LOW);
  digitalWrite(BTN_DOWN_PIN, LOW);

  Serial.println("[Switchs] Button GPIOs initialized (LOW/OFF).");
}

bool triggerButtonPress(HeaterButton btn) {
  // מניעת לחיצה כפולה בזמן שלחצן אחר כבר לחוץ
  if (isPressing) {
    Serial.println("[Switchs] Ignored press request: another button is currently active.");
    return false;
  }

  switch (btn) {
    case BTN_POWER:
      activePin = BTN_POWER_PIN;
      Serial.println("[Switchs] Pressing POWER button (GPIO18)...");
      break;
    case BTN_UP:
      activePin = BTN_UP_PIN;
      Serial.println("[Switchs] Pressing UP button (GPIO19)...");
      break;
    case BTN_DOWN:
      activePin = BTN_DOWN_PIN;
      Serial.println("[Switchs] Pressing DOWN button (GPIO21)...");
      break;
    default:
      return false;
  }

  digitalWrite(activePin, HIGH);
  pressStartTime = millis();
  isPressing = true;
  return true;
}

void handleSwitchs() {
  if (isPressing && (millis() - pressStartTime >= BTN_PRESS_DURATION_MS)) {
    digitalWrite(activePin, LOW);
    isPressing = false;
    activePin = 0;
    Serial.println("[Switchs] Button released.");
  }
}