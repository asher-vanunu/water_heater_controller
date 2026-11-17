#include "MQTTManager.h"
#include "Config.h"
#include "switchs.h"
#include "OTAManager.h"
#include <WiFi.h>
#include <PubSubClient.h>

static WiFiClient espClient;
static PubSubClient mqttClient(espClient);

void publishRawUartHex(const char* hexString) {
  if (WiFi.status() != WL_CONNECTED || !mqttClient.connected()) return;
  mqttClient.publish(TOPIC_UART_RAW_HEX, hexString);
}

static void mqttCallback(char* topic, byte* payload, unsigned int length) {
  char message[32];
  if (length >= sizeof(message)) length = sizeof(message) - 1;
  memcpy(message, payload, length);
  message[length] = '\0';

  Serial.printf("[MQTT] Message arrived [%s]: %s\n", topic, message);

  if (strcmp(topic, TOPIC_CMD_POWER) == 0) {
    if (strcmp(message, "PRESS") == 0 || strcmp(message, "ON") == 0) {
      triggerButtonPress(BTN_POWER);
    }
  } 
  else if (strcmp(topic, TOPIC_CMD_UP) == 0) {
    if (strcmp(message, "PRESS") == 0 || strcmp(message, "ON") == 0) {
      triggerButtonPress(BTN_UP);
    }
  } 
  else if (strcmp(topic, TOPIC_CMD_DOWN) == 0) {
    if (strcmp(message, "PRESS") == 0 || strcmp(message, "ON") == 0) {
      triggerButtonPress(BTN_DOWN);
    }
  } 
  else if (strcmp(topic, TOPIC_CMD_RESET) == 0) {
    delay(100);
    performHardReset();
  }
}

static void reconnectMQTT() {
  static unsigned long lastReconnectAttempt = 0;
  unsigned long now = millis();

  if (now - lastReconnectAttempt > 5000) {
    lastReconnectAttempt = now;

    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("[MQTT] Attempting connection...");
      bool connected = false;

      if (strlen(MQTT_USER) > 0) {
        connected = mqttClient.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD, TOPIC_AVAILABILITY, 1, true, "offline");
      } else {
        connected = mqttClient.connect(MQTT_CLIENT_ID, TOPIC_AVAILABILITY, 1, true, "offline");
      }

      if (connected) {
        Serial.println("Connected!");
        mqttClient.publish(TOPIC_AVAILABILITY, "online", true);

        mqttClient.subscribe(TOPIC_CMD_POWER);
        mqttClient.subscribe(TOPIC_CMD_UP);
        mqttClient.subscribe(TOPIC_CMD_DOWN);
        mqttClient.subscribe(TOPIC_CMD_RESET);
      } else {
        Serial.printf("Failed, rc=%d. Retrying in 5s.\n", mqttClient.state());
      }
    }
  }
}

void setupMQTT() {
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
}

void handleMQTT() {
  if (WiFi.status() != WL_CONNECTED) return;
  if (!mqttClient.connected()) {
    reconnectMQTT();
  } else {
    mqttClient.loop();
  }
}