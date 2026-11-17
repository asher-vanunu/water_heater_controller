#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <WiFi.h>
#include <PubSubClient.h>

void setupMQTT();
void handleMQTT();
void publishRawUartHex(const char* hexString);

#endif // MQTT_MANAGER_H