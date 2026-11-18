#include "Config.h"
#include "switchs.h"
#include "NetworkManager.h"
#include "OTAManager.h"
#include "MQTTManager.h"
#include "UARTManager.h"
#include "WebPortal.h"

void setup() {
  WebSerial.begin(115200);
  delay(500);
  WebSerial.println("\n==========================================");
  WebSerial.println("   Chromagen Water Heater ESP32 Controller");
  WebSerial.println("==========================================");

  setupSwitchs();
  setupUART();
  setupNetwork();
  setupOTA();
  setupMQTT();
  setupWebPortal();
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
  handleWebPortal();
}