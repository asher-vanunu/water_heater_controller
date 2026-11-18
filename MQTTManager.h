#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <WiFi.h>
#include <PubSubClient.h>
#include "UARTManager.h"

void setupMQTT();
void handleMQTT();
void publishRawUartHex(const char* hexString);
void publishHeaterState(const HeaterStatus& status);

#endif // MQTT_MANAGER_H