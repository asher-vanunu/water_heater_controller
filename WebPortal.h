#ifndef WEB_PORTAL_H
#define WEB_PORTAL_H

#include <Arduino.h>
#include <Print.h>

class WebSerialPrint : public Print {
public:
  String buffer;
  bool webOutputEnabled = false;

  size_t write(uint8_t c) override;
  size_t write(const uint8_t *buf, size_t size) override;
  void begin(unsigned long baud);
};

extern WebSerialPrint WebSerial;

void setupWebPortal();
void handleWebPortal();

void setPacketLogEnabled(bool enabled);
bool isPacketLogEnabled();
void appendPacketLog(const String& line);

#endif // WEB_PORTAL_H
