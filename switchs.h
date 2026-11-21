#ifndef SWITCHS_H
#define SWITCHS_H

#include <Arduino.h>

enum HeaterButton {
  BTN_NONE = 0,
  BTN_POWER,
  BTN_UP,
  BTN_DOWN
};

void setupSwitchs();
void handleSwitchs();
bool triggerButtonPress(HeaterButton btn);
void triggerBuzzer();

#endif // SWITCHS_H