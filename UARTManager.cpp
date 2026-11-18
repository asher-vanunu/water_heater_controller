#include "UARTManager.h"
#include "Config.h"
#include "MQTTManager.h"

#define FRAME_LENGTH 10
#define START_BYTE   0x55

static uint8_t rxBuffer[FRAME_LENGTH];
static size_t rxIndex = 0;
static unsigned long lastByteTime = 0;

// שמירת הסטטוס האחרון הידוע לצורך זיהוי שינויים ושידור ל-HA
static HeaterStatus currentStatus = {false, false, false, false, 0, 42, 0, 0, 0, 0, 0};
static unsigned long lastMqttPublishTime = 0;

void setupUART() {
  // הפעלת Pull-up פנימי על פין ה-RX (חיוני כאשר משתמשים בדיודת הגנה 1N4148 ללא נגד חיצוני)
  pinMode(HEATER_UART_RX_PIN, INPUT_PULLUP);

  // אתחול UART2 בקצב 2400 baud (8N1) להאזנה בלבד (TX מוגדר כ--1 כדי למנוע הפרעה לקו בשלב א')
  Serial2.begin(HEATER_UART_BAUD, SERIAL_8N1, HEATER_UART_RX_PIN, -1);
  
  Serial.printf("[UART] מאזין לפרוטוקול Chromagen על פין GPIO%d בקצב %d bps.\n", 
                HEATER_UART_RX_PIN, HEATER_UART_BAUD);
}

HeaterStatus getHeaterStatus() {
  return currentStatus;
}

// אימות Checksum מסוג XOR על בתים 0 עד 8 מול בייט 9
static bool verifyChecksum(const uint8_t* frame) {
  uint8_t xorSum = 0;
  for (int i = 0; i < FRAME_LENGTH - 1; i++) {
    xorSum ^= frame[i];
  }
  return (xorSum == frame[FRAME_LENGTH - 1]);
}

// פענוח פריים תקין והדפסת הנתונים לטרמינל ול-MQTT
static void parseAndPublishFrame(const uint8_t* frame) {
  // עיצוב מחרוזת HEX להדפסה בטרמינל וב-MQTT
  char hexStr[40];
  snprintf(hexStr, sizeof(hexStr), "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
           frame[0], frame[1], frame[2], frame[3], frame[4],
           frame[5], frame[6], frame[7], frame[8], frame[9]);

  // בדיקת Checksum
  if (!verifyChecksum(frame)) {
    Serial.printf("[UART שגיאה] Checksum שגוי בפריים: %s\n", hexStr);
    return;
  }

  // בדיקת כיוון השידור (בייט 1: 0x00 = מהיחידה החיצונית, 0x80 = מהשלט)
  const char* sourceStr = (frame[1] == 0x00) ? "יחידה חיצונית (Master)" : 
                          (frame[1] == 0x80) ? "שלט קיר (Slave)" : "לא ידוע";

  Serial.printf("\n[UART פריים תקין] מקור: %s | HEX: %s\n", sourceStr, hexStr);
  publishRawUartHex(hexStr);

  // אנו מעדכנים את ישות ה-HA רק על סמך תשובות היחידה החיצונית (0x00) המכילות את המדדים האמיתיים
  if (frame[1] != 0x00) {
    return;
  }

  HeaterStatus newStatus;
  newStatus.valid       = true;
  newStatus.statusByte  = frame[2];
  
  // פענוח ביטים של בייט הסטטוס (בייט 2):
  // 0x00 או 0x01 מציינים כיבוי; ערכים כמו 0x02 / 0x12 מציינים מערכת דולקת
  newStatus.isOn        = (frame[2] & 0x02) || (frame[2] > 0x01);
  newStatus.isHeating   = (frame[2] & 0x10) != 0; // ביט 4 (0x10): מבער / להבה פעילה
  newStatus.hasError    = (frame[2] & 0x40) != 0; // ביט 6 (0x40): דגל תקלה
  
  newStatus.targetTemp  = frame[3];               // בייט 3: טמפרטורת יעד
  newStatus.outletTemp  = frame[4];               // בייט 4: טמפרטורת מים יוצאים
  newStatus.errorCode   = frame[5];               // בייט 5: קוד תקלה (0 = תקין)
  newStatus.flowRate    = frame[6];               // בייט 6: ספיקת מים (L/min)
  newStatus.inletTemp   = frame[7];               // בייט 7: טמפרטורת מים נכנסים
  newStatus.burnerPower = frame[8];               // בייט 8: עוצמת מבער (0-100%)

  // הדפסה מפורטת לטרמינל של Arduino IDE לקריאה נוחה בעברית
  Serial.println("--------------------------------------------------");
  Serial.printf("  מצב מערכת: %s | מבער (להבה): %s | סטטוס גולמי: 0x%02X\n",
                newStatus.isOn ? "דולק (ON)" : "כבוי (OFF)",
                newStatus.isHeating ? "פעיל (אש)" : "כבוי",
                newStatus.statusByte);
  Serial.printf("  טמפ' יעד: %d°C | מים יוצאים: %d°C | מים נכנסים: %d°C\n",
                newStatus.targetTemp, newStatus.outletTemp, newStatus.inletTemp);
  Serial.printf("  ספיקת מים: %d L/min | עוצמת מבער: %d%% | קוד תקלה: %d\n",
                newStatus.flowRate, newStatus.burnerPower, newStatus.errorCode);
  Serial.println("--------------------------------------------------");

  // בדיקה האם חל שינוי בנתונים או שחלפו 10 שניות מהשידור האחרון
  bool stateChanged = (
    !currentStatus.valid ||
    currentStatus.isOn        != newStatus.isOn ||
    currentStatus.isHeating   != newStatus.isHeating ||
    currentStatus.hasError    != newStatus.hasError ||
    currentStatus.targetTemp  != newStatus.targetTemp ||
    currentStatus.outletTemp  != newStatus.outletTemp ||
    currentStatus.errorCode   != newStatus.errorCode ||
    currentStatus.flowRate    != newStatus.flowRate ||
    currentStatus.inletTemp   != newStatus.inletTemp ||
    currentStatus.burnerPower != newStatus.burnerPower
  );

  currentStatus = newStatus;

  if (stateChanged || (millis() - lastMqttPublishTime >= 10000)) {
    publishHeaterState(currentStatus);
    lastMqttPublishTime = millis();
  }
}

void handleUART() {
  unsigned long now = millis();

  // איפוס באפר אם עברו יותר מ-60ms באמצע קליטת פריים (מניעת חוסר סנכרון)
  if (rxIndex > 0 && (now - lastByteTime > 60)) {
    rxIndex = 0;
  }

  while (Serial2.available()) {
    uint8_t b = Serial2.read();
    lastByteTime = millis();

    // המתנה לבייט הפתיחה 0x55 בתחילת כל הודעה
    if (rxIndex == 0) {
      if (b == START_BYTE) {
        rxBuffer[rxIndex++] = b;
      }
      continue;
    }

    rxBuffer[rxIndex++] = b;

    // ברגע שהצטברו 10 בתים מלאים - מפענחים מיד
    if (rxIndex == FRAME_LENGTH) {
      parseAndPublishFrame(rxBuffer);
      rxIndex = 0;
    }
  }
}