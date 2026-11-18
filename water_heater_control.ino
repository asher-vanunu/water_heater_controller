#include "Config.h"
#include "switchs.h"
#include "NetworkManager.h"
#include "OTAManager.h"
#include "MQTTManager.h"
#include "UARTManager.h"

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n==========================================");
  Serial.println("   Chromagen Water Heater ESP32 Controller");
  Serial.println("==========================================");

  setupSwitchs();
  setupUART();
  setupNetwork();
  setupOTA();
  setupMQTT();
}

void loop() {
  // טיפול תמידי ב-OTA ובשחרור לחצנים
  handleOTA();
  handleSwitchs();

  // Skip Tasks while OTA to avoid disconnections 
  if (otaInProgress) {
    return;
  }

  checkResetButton();
  handleNetwork();
  handleMQTT();
  handleUART();
}