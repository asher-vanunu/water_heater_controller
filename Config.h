#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ==========================================
// ESP32 HW definitions/pin assignment
// ==========================================
#define BOOT_BUTTON_PIN     0     // Button to reest pairing(3sec press) כפתור BOOT מובנה לאיפוס WiFi (לחיצה 3 שניות)

// Chromagen PLC/UART
#define HEATER_UART_RX_PIN    16    // (RX) Recieved data from the main unit
#define HEATER_UART_TX_PIN    17    // (TX) Transmit data to main unit
#define HEATER_UART_BAUD      4800  // קצב תקשורת מעודכן: 4800 bps (8N1)
#define UART_PACKET_FLAG_PIN  23    // פין חומרה (GPIO23) שעולה ל-HIGH בתחילת פקטה ויורד ל-LOW בסיומה

// GPI for Button switching
#define BTN_POWER_PIN       18    // SW1 - ON/OFF
#define BTN_UP_PIN          19    // SW3 - UP
#define BTN_DOWN_PIN        21    // SW2 - DOWN

// Button press duration
#define BTN_PRESS_DURATION_MS 150

// ==========================================
// הגדרות רשת ו-MQTT (עדכן לפי הרשת שלך)
// ==========================================
#define AP_HOTSPOT_NAME     "WaterHeater-AP"
#define OTA_HOSTNAME        "WaterHeater-ESP32"

#define MQTT_SERVER         "10.100.102.27"  // MQTT server IP address connecting HA with Heater controller
#define MQTT_PORT           1883
#define MQTT_USER           ""               // השאר ריק אם אין שם משתמש
#define MQTT_PASSWORD       ""               // השאר ריק אם אין סיסמה
#define MQTT_CLIENT_ID      "WaterHeater_ESP32"

// ==========================================
// MQTT (Topics)
// ==========================================
#define TOPIC_AVAILABILITY  "water_heater/status"
#define TOPIC_UART_RAW_HEX  "water_heater/uart/raw_hex"
#define TOPIC_STATE_JSON    "water_heater/state"

// פקודות שליטה (זמינות להמשך בשלב ב')
#define TOPIC_CMD_POWER     "water_heater/button/power/set"
#define TOPIC_CMD_UP        "water_heater/button/up/set"
#define TOPIC_CMD_DOWN      "water_heater/button/down/set"
#define TOPIC_CMD_RESET     "water_heater/control/esp_reset/set"

#endif // CONFIG_H