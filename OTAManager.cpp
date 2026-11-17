#include "OTAManager.h"
#include "Config.h"
#include <ArduinoOTA.h>
#include <WiFi.h>
#include <esp_system.h>
#include <esp_sleep.h>
#include <esp_ota_ops.h>

bool otaInProgress = false;

void performHardReset() {
  Serial.println("[System] Performing Deep-Sleep Hard Reset...");
  WiFi.disconnect(true, false);
  WiFi.mode(WIFI_OFF);
  delay(200);

  esp_sleep_enable_timer_wakeup(100000ULL); // 100ms
  esp_deep_sleep_start();
  while (true) {}
}

void setupOTA() {
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.setRebootOnSuccess(true);
  ArduinoOTA.setTimeout(2000);

  ArduinoOTA.onStart([]() {
    String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
    Serial.println("\n[OTA] Update started: " + type);
    Serial.printf("[OTA] Diag: RSSI=%d dBm, FreeHeap=%u bytes\n", WiFi.RSSI(), ESP.getFreeHeap());
    otaInProgress = true;
  });

  ArduinoOTA.onEnd([]() {
    Serial.println("\n[OTA] Update finished successfully.");

    const esp_partition_t* nextPartition = esp_ota_get_next_update_partition(NULL);
    if (nextPartition != NULL) {
      esp_err_t err = esp_ota_set_boot_partition(nextPartition);
      if (err == ESP_OK) {
        Serial.printf("[OTA] Boot partition set to: %s\n", nextPartition->label);
      } else {
        Serial.printf("[OTA] Warning: esp_ota_set_boot_partition error: 0x%X\n", err);
      }
    }

    Serial.println("[OTA] Rebooting into new firmware...");
    otaInProgress = false;
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    unsigned int percent = progress / (total / 100);
    if (percent % 10 == 0) {
      Serial.printf("[OTA] Progress: %u%%\r", percent);
    }
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("[OTA] Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
    otaInProgress = false;
  });

  ArduinoOTA.begin();
  Serial.println("[OTA] Service initialized and listening.");
}

void handleOTA() {
  ArduinoOTA.handle();
}