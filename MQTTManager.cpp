#include "MQTTManager.h"
#include "Config.h"
#include "switchs.h"
#include "OTAManager.h"
#include <WiFi.h>
#include <PubSubClient.h>

static WiFiClient espClient;
static PubSubClient mqttClient(espClient);
static unsigned long mqttConnectedTime = 0;

void publishRawUartHex(const char* hexString) {
  if (WiFi.status() != WL_CONNECTED || !mqttClient.connected()) return;
  mqttClient.publish(TOPIC_UART_RAW_HEX, hexString);
}

// פרסום כל נתוני המחמם כחבילת JSON אחת מסודרת עבור ישות ה-Climate והחיישנים ב-HA
void publishHeaterState(const HeaterStatus& status) {
  if (WiFi.status() != WL_CONNECTED || !mqttClient.connected() || !status.valid) return;

  // קביעת מצבי ישות Climate ב-Home Assistant:
  // mode: "heat" כשהמחמם דולק, "off" כשהמחמם כבוי
  // action: "heating" כשהמבער דולק בפועל, "idle" כשהמחמם דולק בהמתנה למים, "off" כשכבוי
  const char* hvacMode   = status.isOn ? "heat" : "off";
  const char* hvacAction = !status.isOn ? "off" : (status.isHeating ? "heating" : "idle");

  char errorStr[40];
  formatHeaterErrorText(status.errorCode, errorStr, sizeof(errorStr));

  char jsonPayload[320];
  snprintf(jsonPayload, sizeof(jsonPayload),
           "{"
           "\"mode\":\"%s\","
           "\"action\":\"%s\","
           "\"is_on\":\"%s\","
           "\"is_heating\":\"%s\","
           "\"target_temp\":%u,"
           "\"outlet_temp\":%u,"
           "\"inlet_temp\":%u,"
           "\"flow_rate\":%u,"
           "\"burner_power\":%u,"
           "\"error_code\":%u,"
           "\"error_text\":\"%s\","
           "\"status_hex\":\"0x%02X\""
           "}",
           hvacMode,
           hvacAction,
           status.isOn ? "ON" : "OFF",
           status.isHeating ? "ON" : "OFF",
           status.targetTemp,
           status.outletTemp,
           status.inletTemp,
           status.flowRate,
           status.burnerPower,
           status.errorCode,
           errorStr,
           status.statusByte);

  mqttClient.publish(TOPIC_STATE_JSON, jsonPayload, true);
  Serial.printf("[MQTT] פורסם סטטוס מעודכן ל-HA: %s\n", jsonPayload);
}

static void mqttCallback(char* topic, byte* payload, unsigned int length) {
  char message[32];
  if (length >= sizeof(message)) length = sizeof(message) - 1;
  memcpy(message, payload, length);
  message[length] = '\0';

  Serial.printf("[MQTT] Message arrived [%s]: %s\n", topic, message);

  // התעלמות מהודעות Retained ישנות שמגיעות מהברוקר מיד בעת החיבור/עליית ה-ESP32
  if (millis() - mqttConnectedTime < 1500) {
    Serial.println("[MQTT] Ignored startup/retained message during connect grace period.");
    return;
  }

  if (strcmp(topic, TOPIC_CMD_RESET) == 0) {
    triggerBuzzer();
    delay(100);
    performHardReset();
  }
  else if (strcmp(topic, TOPIC_CMD_CLIMATE_MODE) == 0) {
    if (strcmp(message, "heat") == 0) {
      sendHeaterPowerCommand(true);
      triggerBuzzer();
    } else if (strcmp(message, "off") == 0) {
      sendHeaterPowerCommand(false);
      triggerBuzzer();
    }
  }
  else if (strcmp(topic, TOPIC_CMD_CLIMATE_TEMP) == 0) {
    int temp = atoi(message);
    if (temp >= 35 && temp <= 70) {
      sendHeaterTempCommand((uint8_t)temp);
      triggerBuzzer();
    }
  }
  else if (strcmp(topic, TOPIC_CMD_POWER) == 0) {
    if (strcmp(message, "PRESS") == 0 || strcmp(message, "ON") == 0) {
      HeaterStatus st = getHeaterStatus();
      sendHeaterPowerCommand(!st.isOn);
      triggerBuzzer();
    }
  } 
  else if (strcmp(topic, TOPIC_CMD_UP) == 0) {
    if (strcmp(message, "PRESS") == 0 || strcmp(message, "ON") == 0) {
      HeaterStatus st = getHeaterStatus();
      sendHeaterTempCommand(st.targetTemp + 1);
      triggerBuzzer();
    }
  } 
  else if (strcmp(topic, TOPIC_CMD_DOWN) == 0) {
    if (strcmp(message, "PRESS") == 0 || strcmp(message, "ON") == 0) {
      HeaterStatus st = getHeaterStatus();
      if (st.targetTemp > 35) {
        sendHeaterTempCommand(st.targetTemp - 1);
      }
      triggerBuzzer();
    }
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
        mqttConnectedTime = millis();
        mqttClient.publish(TOPIC_AVAILABILITY, "online", true);

        // ניקוי הודעות Retained ישנות אם קיימות על נושאי הפקודות בברוקר
        mqttClient.publish(TOPIC_CMD_POWER, "", true);
        mqttClient.publish(TOPIC_CMD_UP, "", true);
        mqttClient.publish(TOPIC_CMD_DOWN, "", true);
        mqttClient.publish(TOPIC_CMD_CLIMATE_MODE, "", true);
        mqttClient.publish(TOPIC_CMD_CLIMATE_TEMP, "", true);
        mqttClient.publish(TOPIC_CMD_RESET, "", true);

        mqttClient.subscribe(TOPIC_CMD_POWER);
        mqttClient.subscribe(TOPIC_CMD_UP);
        mqttClient.subscribe(TOPIC_CMD_DOWN);
        mqttClient.subscribe(TOPIC_CMD_CLIMATE_MODE);
        mqttClient.subscribe(TOPIC_CMD_CLIMATE_TEMP);
        mqttClient.subscribe(TOPIC_CMD_RESET);

        // פרסום מיידי של הסטטוס האחרון הידוע ברגע החיבור
        HeaterStatus st = getHeaterStatus();
        if (st.valid) {
          publishHeaterState(st);
        }
      } else {
        Serial.printf("Failed, rc=%d. Retrying in 5s.\n", mqttClient.state());
      }
    }
  }
}

void setupMQTT() {
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  // הגדלת באפר ה-MQTT ל-512 בתים כדי להכיל את הודעת ה-JSON המלאה ללא קטיעה
  mqttClient.setBufferSize(512);
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