#include "WebPortal.h"
#include "Config.h"
#include "NetworkManager.h"
#include <WebServer.h>

WebSerialPrint WebSerial;

size_t WebSerialPrint::write(uint8_t c) {
  Serial.write(c);
  if (webOutputEnabled) {
    buffer += (char)c;
    if (buffer.length() > 4096) buffer.remove(0, 2048);
  }
  return 1;
}

size_t WebSerialPrint::write(const uint8_t *buf, size_t size) {
  Serial.write(buf, size);
  if (webOutputEnabled) {
    buffer.reserve(buffer.length() + size);
    for (size_t i = 0; i < size; i++) buffer += (char)buf[i];
    if (buffer.length() > 4096) buffer.remove(0, buffer.length() - 2048);
  }
  return size;
}

void WebSerialPrint::begin(unsigned long baud) {
  Serial.begin(baud);
}

static WebServer portalServer(8080);
static bool packetsLogActive = true;
static String packetsLogBuffer = "";

void setPacketLogEnabled(bool enabled) {
  packetsLogActive = enabled;
}

bool isPacketLogEnabled() {
  return packetsLogActive;
}

void appendPacketLog(const String& line) {
  if (!packetsLogActive) return;
  packetsLogBuffer += line;
  if (packetsLogBuffer.length() > 4096) {
    packetsLogBuffer.remove(0, packetsLogBuffer.length() - 2048);
  }
}

static String getLatestPacketLog() {
  String out = packetsLogBuffer;
  packetsLogBuffer = "";
  return out;
}

static const char* portalPage =
  "<!DOCTYPE html><html><head><meta charset='utf-8'><title>Water Heater Controller</title>"
  "<meta name='viewport' content='width=device-width, initial-scale=1'>"
  "<style>"
  "body{font-family:Arial,sans-serif;display:flex;justify-content:center;align-items:flex-start;gap:20px;min-height:100vh;margin:0;padding:25px;background:#e9ecef;direction:rtl;flex-wrap:wrap;box-sizing:border-box;}"
  ".box{background:white;padding:25px;border-radius:12px;box-shadow:0 4px 15px rgba(0,0,0,0.15);width:350px;box-sizing:border-box;text-align:center;}"
  ".box-log{width:580px;max-width:100%;text-align:right;}"
  "button{color:white;border:none;padding:12px;border-radius:6px;cursor:pointer;font-size:15px;width:100%;font-weight:bold;margin-bottom:10px;transition:0.2s;}"
  "button:hover{opacity:0.9;}"
  "button:disabled{opacity:0.5;cursor:not-allowed;}"
  ".btn-esp{background:#dc3545;color:white;}"
  ".btn-wifi{background:#6f42c1;color:white;}"
  ".btn-start{background:#28a745;color:white;}"
  ".btn-stop{background:#dc3545;color:white;}"
  ".btn-save{background:#17a2b8;color:white;}"
  ".console{background:#111;color:#00ff66;font-family:Consolas,monospace;font-size:12px;padding:12px;border-radius:6px;height:340px;overflow-y:auto;white-space:pre-wrap;text-align:right;direction:rtl;margin:12px 0;border:1px solid #333;}"
  ".console-ltr{text-align:left;direction:ltr;}"
  ".status-indicator{font-size:14px;font-weight:bold;margin-bottom:10px;color:#444;}"
  "</style></head><body>"

  "<!-- מסגרת 1: ניהול מערכת ואיפוסים -->"
  "<div class='box'>"
  "<h2>ניהול בקר מחמם מים ESP32</h2>"
  "<p>Chromagen Water Heater Controller</p>"
  "<button class='btn-esp' onclick=\"if(confirm('לאתחל את ה-ESP32?')) fetch('/reset_esp').then(r=>r.text()).then(alert)\">אתחל ESP32</button>"
  "<hr style='margin:15px 0;'>"
  "<button class='btn-wifi' onclick=\"if(confirm('למחוק הגדרות WiFi ולאתחל?')) { fetch('/reset_wifi'); alert('הגדרות הרשת נמחקו, מאתחל במצב AP...'); }\">איפוס הגדרות WiFi</button>"
  "</div>"

  "<!-- מסגרת 2: מוניטור פקטות UART מפוענחות -->"
  "<div class='box box-log'>"
  "<h2 style='text-align:center;'>מוניטור פקטות UART מפוענחות</h2>"
  "<div id='logStatus' class='status-indicator'>סטטוס: <span style='color:#28a745;'>פעיל</span></div>"
  "<div style='display:flex;gap:10px;'>"
  "<button class='btn-start' id='btnStart' onclick='doStart()' disabled>התחל לוג</button>"
  "<button class='btn-stop' id='btnStop' onclick='doStop()'>הפסק לוג</button>"
  "</div>"
  "<div class='console' id='consoleBox'>[SYSTEM READY] ממתין לפקטות UART מפוענחות...</div>"
  "<div style='display:grid;grid-template-columns:1fr 1fr 1fr;gap:10px;margin-top:10px;'>"
  "<button class='btn-save' style='margin-bottom:0;' onclick='doSave()'>שמור לקובץ</button>"
  "<button style='background:#6c757d;color:white;margin-bottom:0;' onclick='doCopy()'>העתק לוג</button>"
  "<button style='background:#343a40;margin-bottom:0;' onclick='doClear()'>נקה מסך</button>"
  "</div>"
  "</div>"

  "<!-- מסגרת 3: טרמינל סיריאלי ESP32 -->"
  "<div class='box box-log'>"
  "<h2 style='text-align:center;'>ESP32 Serial Terminal</h2>"
  "<div id='termStatus' class='status-indicator'>סטטוס: שידור לפורטל מבוטל</div>"
  "<div style='display:flex;gap:10px;'>"
  "<button class='btn-start' id='btnTermEnable' onclick='doTermToggle(true)'>אפשר שידור לפורטל</button>"
  "<button class='btn-stop' id='btnTermDisable' onclick='doTermToggle(false)' disabled>הפסק שידור</button>"
  "</div>"
  "<div class='console console-ltr' id='terminalBox'>[TERMINAL READY] ממתין לנתונים...</div>"
  "<div style='display:grid;grid-template-columns:1fr 1fr 1fr;gap:10px;margin-top:10px;'>"
  "<button class='btn-save' style='margin-bottom:0;' onclick='doSaveTerminal()'>שמור לקובץ</button>"
  "<button style='background:#6c757d;color:white;margin-bottom:0;' onclick='doCopyTerminal()'>העתק טרמינל</button>"
  "<button style='background:#343a40;margin-bottom:0;' onclick='doClearTerminal()'>נקה מסך</button>"
  "</div>"
  "</div>"

  "<script>"
  "var logData = ''; var termData = ''; var timer = setInterval(poll, 1000); var termEnabled = false;"
  "function doStart() {"
  "  fetch('/packets/start').then(function(){"
  "    document.getElementById('logStatus').innerHTML = 'סטטוס: <span style=\"color:#28a745;\">פעיל</span>';"
  "    document.getElementById('btnStart').disabled = true;"
  "    document.getElementById('btnStop').disabled = false;"
  "    if(!timer) timer = setInterval(poll, 1000);"
  "  });"
  "}"
  "function doStop() {"
  "  fetch('/packets/stop').then(function(){"
  "    document.getElementById('logStatus').innerHTML = 'סטטוס: <span style=\"color:#dc3545;\">מושבת</span>';"
  "    document.getElementById('btnStart').disabled = false;"
  "    document.getElementById('btnStop').disabled = true;"
  "    if(timer) { clearInterval(timer); timer = null; }"
  "  });"
  "}"
  "function poll() {"
  "  fetch('/packets/read?t=' + new Date().getTime()).then(r=>r.text()).then(function(txt){"
  "    if(txt && txt.length > 0) {"
  "      logData += txt;"
  "      var c = document.getElementById('consoleBox');"
  "      if(c.textContent.indexOf('[SYSTEM READY') !== -1) c.textContent = '';"
  "      c.textContent += txt;"
  "      c.scrollTop = c.scrollHeight;"
  "    }"
  "  });"
  "}"
  "function doClear() { logData = ''; document.getElementById('consoleBox').textContent = ''; }"
  "function doCopy() { navigator.clipboard.writeText(logData).then(function(){ alert('הועתק ללוח!'); }); }"
  "function doSave() {"
  "  if(!logData.trim()) return;"
  "  var a = document.createElement('a');"
  "  a.href = URL.createObjectURL(new Blob([logData], { type: 'text/plain;charset=utf-8' }));"
  "  a.download = 'WaterHeater_Packets_Log.txt'; a.click();"
  "}"
  "function doTermToggle(state) {"
  "  fetch('/terminal/toggle?state=' + (state ? '1' : '0') + '&t=' + new Date().getTime()).then(function(){"
  "    termEnabled = state;"
  "    document.getElementById('termStatus').innerHTML = state ? 'סטטוס: <span style=\"color:#28a745;\">שידור מופעל</span>' : 'סטטוס: <span style=\"color:#dc3545;\">שידור מבוטל</span>';"
  "    document.getElementById('btnTermEnable').disabled = state;"
  "    document.getElementById('btnTermDisable').disabled = !state;"
  "  });"
  "}"
  "function pollTerminal() {"
  "  if(!termEnabled) return;"
  "  fetch('/terminal/read?t=' + new Date().getTime()).then(r=>r.text()).then(function(txt){"
  "    if(txt && txt.length > 0) {"
  "      termData += txt;"
  "      var c = document.getElementById('terminalBox');"
  "      if(c.textContent.indexOf('[TERMINAL READY') !== -1) c.textContent = '';"
  "      c.textContent += txt;"
  "      c.scrollTop = c.scrollHeight;"
  "    }"
  "  });"
  "}"
  "setInterval(pollTerminal, 1200);"
  "function doSaveTerminal() {"
  "  if(!termData.trim()) return;"
  "  var a = document.createElement('a');"
  "  a.href = URL.createObjectURL(new Blob([termData], { type: 'text/plain;charset=utf-8' }));"
  "  a.download = 'WaterHeater_Terminal_Dump.txt'; a.click();"
  "}"
  "function doCopyTerminal() { navigator.clipboard.writeText(termData).then(function(){ alert('הועתק ללוח!'); }); }"
  "function doClearTerminal() { termData = ''; document.getElementById('terminalBox').textContent = ''; }"
  "</script></body></html>";

void setupWebPortal() {
  portalServer.on("/", HTTP_GET, []() {
    portalServer.sendHeader("Cache-Control", "no-cache");
    portalServer.send(200, "text/html", portalPage);
  });

  portalServer.on("/reset_esp", HTTP_GET, []() {
    portalServer.send(200, "text/plain", "ESP32 Rebooting...");
    delay(500);
    ESP.restart();
  });

  portalServer.on("/reset_wifi", HTTP_GET, []() {
    portalServer.send(200, "text/plain", "WiFi Cleared. Restarting...");
    delay(500);
    resetWiFiSettings();
    delay(1000);
    ESP.restart();
  });

  portalServer.on("/packets/start", HTTP_GET, []() {
    setPacketLogEnabled(true);
    portalServer.send(200, "text/plain", "STARTED");
  });

  portalServer.on("/packets/stop", HTTP_GET, []() {
    setPacketLogEnabled(false);
    portalServer.send(200, "text/plain", "STOPPED");
  });

  portalServer.on("/packets/read", HTTP_GET, []() {
    String logText = getLatestPacketLog();
    portalServer.sendHeader("Cache-Control", "no-cache");
    portalServer.send(200, "text/plain", logText);
  });

  portalServer.on("/terminal/toggle", HTTP_GET, []() {
    if (portalServer.hasArg("state")) {
      WebSerial.webOutputEnabled = (portalServer.arg("state") == "1");
      WebSerial.buffer = "";
    }
    portalServer.send(200, "text/plain", WebSerial.webOutputEnabled ? "ENABLED" : "DISABLED");
  });

  portalServer.on("/terminal/read", HTTP_GET, []() {
    portalServer.sendHeader("Cache-Control", "no-cache");
    portalServer.send(200, "text/plain", WebSerial.buffer);
    WebSerial.buffer = "";
  });

  portalServer.begin();
  WebSerial.println("[WebPortal] Water Heater Portal running on port 8080");
}

void handleWebPortal() {
  portalServer.handleClient();
}
