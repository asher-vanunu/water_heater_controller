#include "NetworkManager.h"
#include "Config.h"

WiFiManager wm;

static unsigned long buttonPressStart = 0;
static bool buttonStatePrevious = HIGH;
static bool wasConnected = false; 
static unsigned long lastReconnectAttempt = 0;

static void configureWiFiCountry() {
  wifi_country_t country = {
    .cc = "IL",
    .schan = 1,
    .nchan = 13,
    .max_tx_power = 20,
    .policy = WIFI_COUNTRY_POLICY_AUTO
  };
  esp_wifi_set_country(&country);
}

void setupNetwork() {
  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
  WiFi.persistent(true); 
  delay(200);

  WiFi.mode(WIFI_STA); 
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  
  configureWiFiCountry();
  
  wm.setConfigPortalBlocking(false); 
  wm.setConfigPortalTimeout(180);     
  wm.setConnectTimeout(20); 

  Serial.println("[Network] Starting network connection process...");

  bool res = wm.autoConnect(AP_HOTSPOT_NAME);

  if (!res) {
    Serial.print("[Network] Could not connect immediately. Config Portal '");
    Serial.print(AP_HOTSPOT_NAME);
    Serial.println("' started in background (IP: 192.168.4.1).");
    wasConnected = false;
  } else {
    Serial.println("\n[Network] WiFi Connected successfully on boot!");
    Serial.printf("[Network] SSID: %s | IP: %s | RSSI: %d dBm\n", 
                  WiFi.SSID().c_str(), WiFi.localIP().toString().c_str(), WiFi.RSSI());
    wasConnected = true;
  }
}

void handleNetwork() {
  wm.process();

  bool isConnected = (WiFi.status() == WL_CONNECTED);
  unsigned long currentMillis = millis();

  if (isConnected && !wasConnected) {
    Serial.println("\n---------------------------------------------");
    Serial.println("[Network Status] WiFi Connected Successfully!");
    Serial.printf("[Network Status] SSID: %s | IP: %s | RSSI: %d dBm\n", 
                  WiFi.SSID().c_str(), WiFi.localIP().toString().c_str(), WiFi.RSSI());
    Serial.println("---------------------------------------------\n");
    wasConnected = true;
  } 
  else if (!isConnected && wasConnected) {
    Serial.println("\n---------------------------------------------");
    Serial.println("[Network Status] WiFi Disconnected!");
    Serial.println("---------------------------------------------\n");
    wasConnected = false;
    lastReconnectAttempt = currentMillis; 
  }

  if (!isConnected && (currentMillis - lastReconnectAttempt >= 10000)) {
    lastReconnectAttempt = currentMillis;
    if (!wm.getConfigPortalActive()) {
      Serial.println("[Network] Attempting active background reconnect to WiFi...");
      WiFi.reconnect(); 
    }
  }
}

void checkResetButton() {
  bool buttonStateCurrent = digitalRead(BOOT_BUTTON_PIN);

  if (buttonStateCurrent == LOW && buttonStatePrevious == HIGH) {
    buttonPressStart = millis();
  }

  if (buttonStateCurrent == LOW && (millis() - buttonPressStart > 3000)) {
    Serial.println("\n[RESET] BOOT Button held for 3s! Clearing Wi-Fi credentials...");
    wm.resetSettings();
    WiFi.disconnect(true, true);
    delay(1000);
    ESP.restart();
  }

  buttonStatePrevious = buttonStateCurrent;
}

void resetWiFiSettings() {
  wm.resetSettings();
  WiFi.disconnect(true, true);
  Serial.println("[Network] Wi-Fi settings reset manually via code.");
}