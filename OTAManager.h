#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <Arduino.h>

void setupOTA();
void handleOTA();
void performHardReset();

extern bool otaInProgress;

#endif // OTA_MANAGER_H