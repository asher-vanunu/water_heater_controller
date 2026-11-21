#ifndef UART_MANAGER_H
#define UART_MANAGER_H

#include <Arduino.h>

// מבנה נתונים המייצג את כל המדדים המפוענחים מפרוטוקול Chromagen (קו CN1)
struct HeaterStatus {
  bool valid;               // האם התקבל פריים תקין (Checksum מאומת)
  bool isOn;                // מצב מערכת: דולק / כבוי (Byte 4)
  bool isHeating;           // מצב מבער: בעירה פעילה (Byte 7 == 0x03 או 0x07)
  bool hasError;            // דגל תקלה (Byte 8 != 0x00)
  uint8_t msgType;          // סוג הודעה (Byte 2: 0x00=שוטף, 0x21=ACK, 0x10..0x13=סריקה)
  uint8_t statusByte;       // מכונת מצבים / בעירה (Byte 7)
  uint8_t targetTemp;       // טמפרטורת יעד מוגדרת (°C) - Byte 9
  uint8_t outletTemp;       // טמפרטורת מים יוצאים בפועל (°C) - Byte 21
  uint8_t errorCode;        // קוד תקלה (Byte 8: 0x00=תקין, 0x93=C0)
  uint8_t flowRate;         // אינדיקציית זרימת מים (נגזר מ-Byte 7)
  uint8_t inletTemp;        // טמפרטורת מי כניסה (°C) - Byte 13
  uint8_t bodyTemp;         // טמפרטורת חיישן גוף / משני (°C) - Byte 14
  uint8_t burnerPower;      // עוצמת מפוח / Fan PWM (0x00, 0x58..0x75) - Byte 26
};

void setupUART();
void handleUART();
HeaterStatus getHeaterStatus();
void formatHeaterErrorText(uint8_t errorCode, char* out, size_t outSize);
void sendHeaterPowerCommand(bool turnOn);
void sendHeaterTempCommand(uint8_t targetTemp);

#endif // UART_MANAGER_H