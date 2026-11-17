#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <WiFi.h>
#include <esp_wifi.h>
#include <WiFiManager.h>

void setupNetwork();
void handleNetwork();

void checkResetButton();
void resetWiFiSettings();

#endif // NETWORK_MANAGER_H