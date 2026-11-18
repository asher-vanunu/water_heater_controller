#ifndef UART_MANAGER_H
#define UART_MANAGER_H

#include <Arduino.h>

// מבנה נתונים המייצג את כל המדדים המפוענחים מהמחמם (פרק 3 בתיעוד)
struct HeaterStatus {
  bool valid;               // האם התקבל פריים תקין (Checksum מאומת)
  bool isOn;                // מצב מערכת: דולק / כבוי (בייט 2)
  bool isHeating;           // מצב מבער: אש דולקת / חימום פעיל (בייט 2, ביט 4)
  bool hasError;            // דגל תקלה כללי (בייט 2, ביט 6)
  uint8_t statusByte;       // ערך גולמי של בייט הסטטוס לדיבאג
  uint8_t targetTemp;       // טמפרטורת יעד מוגדרת (°C) - בייט 3
  uint8_t outletTemp;       // טמפרטורת מים יוצאים בפועל (°C) - בייט 4
  uint8_t errorCode;        // קוד תקלה (לדוגמה 11 = E11) - בייט 5
  uint8_t flowRate;         // ספיקת מים (L/min) - בייט 6
  uint8_t inletTemp;        // טמפרטורת מים נכנסים (°C) - בייט 7
  uint8_t burnerPower;      // עוצמת מבער / פתיחת סולנואיד (0-100%) - בייט 8
};

void setupUART();
void handleUART();
HeaterStatus getHeaterStatus();

#endif // UART_MANAGER_H