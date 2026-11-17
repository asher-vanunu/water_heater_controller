#include "UARTManager.h"
#include "Config.h"
#include "MQTTManager.h"

#define UART_BUF_SIZE 64
static uint8_t uartBuffer[UART_BUF_SIZE];
static size_t uartIndex = 0;
static unsigned long lastByteTime = 0;

void setupUART() {
  // שימוש ב-HardwareSerial 2 של ה-ESP32 על GPIO16 (RX) ו-GPIO17 (TX)
  Serial2.begin(HEATER_UART_BAUD, SERIAL_8N1, HEATER_UART_RX_PIN, HEATER_UART_TX_PIN);
  Serial.printf("[UART] Serial2 initialized on RX=%d, TX=%d at %d baud.\n", 
                HEATER_UART_RX_PIN, HEATER_UART_TX_PIN, HEATER_UART_BAUD);
}

void handleUART() {
  while (Serial2.available()) {
    uint8_t b = Serial2.read();
    lastByteTime = millis();

    if (uartIndex < UART_BUF_SIZE) {
      uartBuffer[uartIndex++] = b;
    }
  }

  // זיהוי סוף פריים לאחר 50ms של שקט בקו (זהה להגדרת timeout: 50ms ב-ESPHome)
  if (uartIndex > 0 && (millis() - lastByteTime >= 50)) {
    char hexStr[UART_BUF_SIZE * 3 + 1];
    size_t pos = 0;

    for (size_t i = 0; i < uartIndex; i++) {
      pos += snprintf(&hexStr[pos], sizeof(hexStr) - pos, "%02X%s", 
                      uartBuffer[i], (i < uartIndex - 1) ? ":" : "");
    }

    Serial.printf("[UART RX] %s\n", hexStr);
    publishRawUartHex(hexStr);

    uartIndex = 0;
  }
}