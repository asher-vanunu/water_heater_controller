#include "UARTManager.h"
#include "Config.h"
#include "MQTTManager.h"
#include "WebPortal.h"

#define HEADER_STATUS_FRAME   0xF5
#define STATUS_FRAME_LEN      31
#define STATUS_PAYLOAD_LEN    0x1E  // 30 bytes after Byte 0

#define HEADER_COMMAND_FRAME  0xFA
#define COMMAND_FRAME_LEN     11
#define COMMAND_PAYLOAD_LEN   0x0A  // 10 bytes after Byte 0

#define MAX_FRAME_LEN         STATUS_FRAME_LEN

static uint8_t rxBuffer[MAX_FRAME_LEN];
static size_t rxIndex = 0;
static size_t expectedFrameLen = 0;
static unsigned long lastByteTime = 0;
static bool rawLineActive = false;
static uint8_t rawByteCount = 0;

// שמירת הסטטוס האחרון הידוע לצורך זיהוי שינויים ושידור ל-HA
static HeaterStatus currentStatus = {false, false, false, false, 0, 0, 42, 0, 0, 0, 0, 0, 0};
static unsigned long lastMqttPublishTime = 0;

void setupUART() {
  // הפעלת Pull-up פנימי על פין ה-RX
  pinMode(HEATER_UART_RX_PIN, INPUT_PULLUP);

  // הגדרת פין חיווי/דגל חומרה לקליטת פקטות (HIGH בתחילת פקטה, LOW בסיומה)
  pinMode(UART_PACKET_FLAG_PIN, OUTPUT);
  digitalWrite(UART_PACKET_FLAG_PIN, LOW);

  // אתחול UART2 בקצב 4800 baud (8N1) להאזנה בלבד (TX מוגדר כ--1 בשלב זה)
  Serial2.begin(HEATER_UART_BAUD, SERIAL_8N1, HEATER_UART_RX_PIN, -1);

  WebSerial.printf("[UART] מאזין לפרוטוקול Chromagen CN1 (0xF5/0xFA) על GPIO%d בקצב %d bps (דגל פקטה: GPIO%d).\n",
                   HEATER_UART_RX_PIN, HEATER_UART_BAUD, UART_PACKET_FLAG_PIN);
}

HeaterStatus getHeaterStatus() {
  return currentStatus;
}

void formatHeaterErrorText(uint8_t errorCode, char* out, size_t outSize) {
  if (errorCode == 0x00) {
    snprintf(out, outSize, "תקין");
  } else if (errorCode == 0x93) {
    snprintf(out, outSize, "C0 (E1) כשל הצתה");
  } else {
    snprintf(out, outSize, "תקלה 0x%02X", errorCode);
  }
}

// אימות Checksum מסוג Two's Complement:
// Checksum = (256 - (Sum of Bytes [1 .. N-2] mod 256)) mod 256
static bool verifyChecksum(const uint8_t* frame, size_t len) {
  if (len < 3) return false;
  uint16_t sum = 0;
  for (size_t i = 1; i < len - 1; i++) {
    sum += frame[i];
  }
  uint8_t calc = (uint8_t)((256 - (sum & 0xFF)) & 0xFF);
  return (calc == frame[len - 1]);
}

static const char* combustionStateName(uint8_t state) {
  switch (state) {
    case 0x00: return "המתנה (Idle)";
    case 0x01: return "זרימת מים + אוורור מקדים";
    case 0x02: return "אוורור לאחר כיבוי (Post-purge)";
    case 0x03: return "מבער דולק (Ignition/Burner)";
    case 0x07: return "בעירה פעילה (Burning)";
    default:   return "לא ידוע";
  }
}

static const char* msgTypeName(uint8_t type) {
  if (type == 0x00) return "שידור שוטף";
  if (type == 0x21) return "אישור פקודה (ACK)";
  if (type >= 0x10 && type <= 0x13) return "סריקת מערכת";
  return "לא ידוע";
}

// עיצוב מחרוזת HEX באורך דינמי
static void formatHexString(const uint8_t* frame, size_t len, char* out, size_t outSize) {
  size_t pos = 0;
  for (size_t i = 0; i < len && pos + 4 < outSize; i++) {
    pos += snprintf(out + pos, outSize - pos, (i + 1 < len) ? "%02X " : "%02X", frame[i]);
  }
}

// פענוח מסגרת פקודה 0xFA (11 בתים) מהשלט
static void parseCommandFrame(const uint8_t* frame, const char* hexStr, float uptimeSec) {
  uint8_t cmdId = frame[3];
  uint8_t powerParam = frame[4];
  uint8_t tempParam = frame[5];

  const char* cmdDesc = "פקודה לא ידועה";
  char detail[64] = "";

  if (cmdId == 0x58) {
    cmdDesc = (powerParam == 0x01) ? "הדלקת מחמם (Power ON)" : "כיבוי מחמם (Power OFF)";
  } else if (cmdId == 0x08) {
    cmdDesc = "כיבוי מחמם (Power OFF)";
  } else if (cmdId == 0x18) {
    cmdDesc = "קביעת טמפרטורה";
    snprintf(detail, sizeof(detail), " | יעד: %d°C", tempParam);
  } else if (cmdId == 0x28) {
    cmdDesc = "איפוס תקלה";
  }

  WebSerial.printf("\n[UART פקודת שלט 0xFA] %s%s | HEX: %s\n", cmdDesc, detail, hexStr);
  publishRawUartHex(hexStr);

  if (isPacketLogEnabled()) {
    char portalBuf[256];
    snprintf(portalBuf, sizeof(portalBuf),
             "[%.1fs] [פקודת שלט 0xFA] %s%s\n  HEX: %s\n--------------------------------------------------\n",
             uptimeSec, cmdDesc, detail, hexStr);
    appendPacketLog(String(portalBuf));
  }
}

// פענוח מסגרת סטטוס 0xF5 (31 בתים) מהיחידה החיצונית
static void parseStatusFrame(const uint8_t* frame, const char* hexStr, float uptimeSec) {
  WebSerial.printf("\n[UART סטטוס יחידה חיצונית 0xF5] (%s) | HEX: %s\n", msgTypeName(frame[2]), hexStr);
  publishRawUartHex(hexStr);

  HeaterStatus newStatus;
  newStatus.valid       = true;
  newStatus.msgType     = frame[2];                   // Byte 2: Message Type
  newStatus.isOn        = (frame[4] == 0x01);         // Byte 4: Power State (0x00=OFF, 0x01=ON)
  newStatus.statusByte  = frame[7];                   // Byte 7: Combustion State Machine
  newStatus.isHeating   = (frame[7] == 0x03 || frame[7] == 0x07);
  newStatus.flowRate    = (frame[7] == 0x01 || frame[7] == 0x03 || frame[7] == 0x07) ? 1 : 0;
  newStatus.errorCode   = frame[8];                   // Byte 8: Error Code (0x00=תקין, 0x93=C0)
  newStatus.hasError    = (frame[8] != 0x00);
  newStatus.targetTemp  = frame[9];                   // Byte 9: Target Temp (°C)
  newStatus.inletTemp   = frame[13];                  // Byte 13: Inlet Temp (°C)
  newStatus.bodyTemp    = frame[14];                  // Byte 14: Body / Secondary Sensor Temp (°C)
  newStatus.outletTemp  = frame[21];                  // Byte 21: Outlet Temp (°C)
  newStatus.burnerPower = frame[26];                  // Byte 26: Fan PWM (0x00, 0x58..0x75)

  const char* stateDesc = combustionStateName(newStatus.statusByte);
  char errorDesc[40];
  formatHeaterErrorText(newStatus.errorCode, errorDesc, sizeof(errorDesc));

  WebSerial.println("--------------------------------------------------");
  WebSerial.printf("  מצב מערכת: %s | מכונת מצבים: %s (0x%02X)\n",
                   newStatus.isOn ? "דולק (ON)" : "כבוי (OFF)",
                   stateDesc, newStatus.statusByte);
  WebSerial.printf("  טמפ' יעד: %d°C | מים יוצאים: %d°C | מים נכנסים: %d°C | חיישן גוף: %d°C\n",
                   newStatus.targetTemp, newStatus.outletTemp, newStatus.inletTemp, newStatus.bodyTemp);
  WebSerial.printf("  עוצמת מפוח (PWM): 0x%02X (%d) | קוד תקלה: 0x%02X (%s)\n",
                   newStatus.burnerPower, newStatus.burnerPower, newStatus.errorCode, errorDesc);
  WebSerial.println("--------------------------------------------------");

  if (isPacketLogEnabled()) {
    char portalBuf[480];
    snprintf(portalBuf, sizeof(portalBuf),
             "[%.1fs] [סטטוס 0xF5 | %s]\n"
             "  מצב: %s | בעירה: %s (0x%02X) | תקלה: 0x%02X (%s)\n"
             "  יעד: %d°C | יוצאים: %d°C | נכנסים: %d°C | גוף: %d°C | מפוח PWM: 0x%02X\n"
             "  HEX: %s\n"
             "--------------------------------------------------\n",
             uptimeSec, msgTypeName(newStatus.msgType),
             newStatus.isOn ? "דולק (ON)" : "כבוי (OFF)",
             stateDesc, newStatus.statusByte,
             newStatus.errorCode, errorDesc,
             newStatus.targetTemp, newStatus.outletTemp, newStatus.inletTemp, newStatus.bodyTemp,
             newStatus.burnerPower,
             hexStr);
    appendPacketLog(String(portalBuf));
  }

  // בדיקה האם חל שינוי בנתונים או שחלפו 10 שניות מהשידור האחרון
  bool stateChanged = (
    !currentStatus.valid ||
    currentStatus.isOn        != newStatus.isOn ||
    currentStatus.isHeating   != newStatus.isHeating ||
    currentStatus.hasError    != newStatus.hasError ||
    currentStatus.statusByte  != newStatus.statusByte ||
    currentStatus.targetTemp  != newStatus.targetTemp ||
    currentStatus.outletTemp  != newStatus.outletTemp ||
    currentStatus.errorCode   != newStatus.errorCode ||
    currentStatus.inletTemp   != newStatus.inletTemp ||
    currentStatus.bodyTemp    != newStatus.bodyTemp ||
    currentStatus.burnerPower != newStatus.burnerPower
  );

  currentStatus = newStatus;

  if (stateChanged || (millis() - lastMqttPublishTime >= 10000)) {
    publishHeaterState(currentStatus);
    lastMqttPublishTime = millis();
  }
}

// פענוח פריים מלא (0xF5 או 0xFA)
static void parseAndPublishFrame(const uint8_t* frame, size_t len) {
  char hexStr[120];
  formatHexString(frame, len, hexStr, sizeof(hexStr));

  float uptimeSec = millis() / 1000.0f;

  if (!verifyChecksum(frame, len)) {
    WebSerial.printf("[UART שגיאה] Checksum שגוי (אורך %u): %s\n", (unsigned)len, hexStr);
    if (isPacketLogEnabled()) {
      char errBuf[200];
      snprintf(errBuf, sizeof(errBuf), "[%.1fs] [שגיאת Checksum | אורך %u] HEX: %s\n",
               uptimeSec, (unsigned)len, hexStr);
      appendPacketLog(String(errBuf));
    }
    return;
  }

  if (frame[0] == HEADER_STATUS_FRAME) {
    parseStatusFrame(frame, hexStr, uptimeSec);
  } else if (frame[0] == HEADER_COMMAND_FRAME) {
    parseCommandFrame(frame, hexStr, uptimeSec);
  }
}

void handleUART() {
  unsigned long now = millis();

  // איפוס באפר אם עברו יותר מ-60ms באמצע קליטת פריים (מניעת חוסר סנכרון)
  if (now - lastByteTime > 60) {
    if (rawLineActive) {
      WebSerial.println();
      rawLineActive = false;
      rawByteCount = 0;
    }
    if (rxIndex > 0) {
      rxIndex = 0;
      expectedFrameLen = 0;
      digitalWrite(UART_PACKET_FLAG_PIN, LOW);
    }
  }

  while (Serial2.available()) {
    uint8_t b = Serial2.read();
    lastByteTime = millis();

    // הדפסה גולמית של כל בייט שמגיע מה-UART (גם ללא פענוח)
    if (!rawLineActive) {
      WebSerial.print("[UART RAW] ");
      rawLineActive = true;
      rawByteCount = 0;
    }
    WebSerial.printf("%02X ", b);
    rawByteCount++;
    if (rawByteCount >= 16) {
      WebSerial.println();
      rawLineActive = false;
      rawByteCount = 0;
    }

    // המתנה לבייט הפתיחה 0xF5 (סטטוס, 31 בתים) או 0xFA (פקודה, 11 בתים)
    if (rxIndex == 0) {
      if (b == HEADER_STATUS_FRAME) {
        expectedFrameLen = STATUS_FRAME_LEN;
        digitalWrite(UART_PACKET_FLAG_PIN, HIGH);
        rxBuffer[rxIndex++] = b;
      } else if (b == HEADER_COMMAND_FRAME) {
        expectedFrameLen = COMMAND_FRAME_LEN;
        digitalWrite(UART_PACKET_FLAG_PIN, HIGH);
        rxBuffer[rxIndex++] = b;
      }
      continue;
    }

    // אימות בייט האורך הצפוי (Byte 1) לסנכרון מהיר
    if (rxIndex == 1) {
      uint8_t expectedLenByte = (rxBuffer[0] == HEADER_STATUS_FRAME) ? STATUS_PAYLOAD_LEN : COMMAND_PAYLOAD_LEN;
      if (b != expectedLenByte) {
        // בייט 1 לא תואם - מאפסים סנכרון ובודקים אם הבייט הנוכחי הוא פתיח חדש
        rxIndex = 0;
        expectedFrameLen = 0;
        digitalWrite(UART_PACKET_FLAG_PIN, LOW);
        if (b == HEADER_STATUS_FRAME || b == HEADER_COMMAND_FRAME) {
          expectedFrameLen = (b == HEADER_STATUS_FRAME) ? STATUS_FRAME_LEN : COMMAND_FRAME_LEN;
          digitalWrite(UART_PACKET_FLAG_PIN, HIGH);
          rxBuffer[rxIndex++] = b;
        }
        continue;
      }
    }

    rxBuffer[rxIndex++] = b;

    // ברגע שהצטברו כל בתי הפריים - מורידים את דגל החומרה ומפענחים מיד
    if (rxIndex == expectedFrameLen) {
      digitalWrite(UART_PACKET_FLAG_PIN, LOW);
      if (rawLineActive) {
        WebSerial.println();
        rawLineActive = false;
        rawByteCount = 0;
      }
      parseAndPublishFrame(rxBuffer, expectedFrameLen);
      rxIndex = 0;
      expectedFrameLen = 0;
    }
  }
}
