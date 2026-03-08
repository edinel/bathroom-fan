// Bathroom Fan Controller - Web Server
//
// Style follows Christmas-PIO: PsychicHttp + TemplatePrinter, arduino-secrets.h,
// GET params for control, JSON endpoint for live JS polling.
//
// To integrate:
//   1. Add to platformio.ini lib_deps:
//        https://github.com/hoeken/PsychicHttp
//        https://github.com/sticilface/TemplatePrinter  (or Arduino Library Manager)
//   2. Create src/arduino-secrets.h with:
//        const char* ssid = "YourSSID";
//        const char* pass = "YourPassword";
//   3. Declare these in main.cpp and mark extern here (see bottom of file)
//   4. Call setupWebServer() from setup(), handleWebServer() from loop()

#include <Arduino.h>
#include <WiFi.h>
#include <PsychicHttp.h>
#include <TemplatePrinter.h>
#include "arduino-secrets.h"

#define HOSTNAME "bathroom-fan"

PsychicHttpServer server;

// ---------------------------------------------------------------------------
// Shared state — main.cpp owns these, we reference them
// ---------------------------------------------------------------------------
extern float   g_humidity;      // latest humidity reading (%)
extern float   g_temperature;   // latest temperature reading (C)
extern int     g_fanRPM;        // latest fan RPM
extern int     g_dutyCycle;     // current duty cycle being applied (0-100)

// Manual override — web UI can force a duty cycle, bypassing humidity logic
bool g_manualOverride = false;
int  g_manualDuty     = 0;

// ---------------------------------------------------------------------------
// HTML + CSS (inline here; move to string_constants.h if it gets large)
// ---------------------------------------------------------------------------
static const char INDEX_HTML[] = R"rawhtml(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Bathroom Fan</title>
  <style>
    body { font-family: sans-serif; max-width: 480px; margin: 40px auto; padding: 0 16px; background: #f4f4f4; }
    h1   { font-size: 1.4em; color: #333; }
    .card { background: white; border-radius: 8px; padding: 16px; margin-bottom: 16px; box-shadow: 0 1px 4px rgba(0,0,0,0.1); }
    .stat { display: flex; justify-content: space-between; padding: 6px 0; border-bottom: 1px solid #eee; }
    .stat:last-child { border-bottom: none; }
    .label { color: #666; }
    .value { font-weight: bold; color: #222; }
    .mode-bar { display: flex; gap: 8px; margin-bottom: 12px; }
    .mode-bar button { flex: 1; padding: 10px; border: none; border-radius: 6px; cursor: pointer; font-size: 1em; }
    #btn-auto   { background: %AUTO_ACTIVE%;   color: white; }
    #btn-manual { background: %MANUAL_ACTIVE%; color: white; }
    .speed-buttons { display: flex; gap: 8px; flex-wrap: wrap; }
    .speed-buttons button { flex: 1; min-width: 60px; padding: 10px; border: none; border-radius: 6px;
                            background: #607d8b; color: white; cursor: pointer; font-size: 0.95em; }
    .speed-buttons button:hover { background: #455a64; }
    #manual-controls { display: %MANUAL_DISPLAY%; }
    #status-dot { display: inline-block; width: 10px; height: 10px; border-radius: 50%;
                  background: #4caf50; margin-right: 6px; }
  </style>
</head>
<body>
  <h1><span id="status-dot"></span>Bathroom Fan</h1>

  <div class="card">
    <div class="stat"><span class="label">Humidity</span>    <span class="value" id="humidity">%HUMIDITY%</span></div>
    <div class="stat"><span class="label">Temperature</span> <span class="value" id="temperature">%TEMPERATURE%</span></div>
    <div class="stat"><span class="label">Fan Speed</span>   <span class="value" id="duty">%DUTY%</span></div>
    <div class="stat"><span class="label">Fan RPM</span>     <span class="value" id="rpm">%RPM%</span></div>
  </div>

  <div class="card">
    <div class="mode-bar">
      <button id="btn-auto"   onclick="setMode('auto')">Auto</button>
      <button id="btn-manual" onclick="setMode('manual')">Manual</button>
    </div>
    <div id="manual-controls">
      <p style="color:#666; margin:0 0 8px">Set fan speed:</p>
      <div class="speed-buttons">
        <button onclick="setSpeed(0)">Off</button>
        <button onclick="setSpeed(25)">25%</button>
        <button onclick="setSpeed(50)">50%</button>
        <button onclick="setSpeed(75)">75%</button>
        <button onclick="setSpeed(100)">100%</button>
      </div>
    </div>
  </div>

  <script>
    function setMode(mode) {
      fetch('/update?mode=' + mode).then(() => refreshStatus());
    }
    function setSpeed(pct) {
      fetch('/update?duty=' + pct).then(() => refreshStatus());
    }
    function refreshStatus() {
      fetch('/status')
        .then(r => r.json())
        .then(d => {
          document.getElementById('humidity').textContent    = d.humidity.toFixed(1) + ' %';
          document.getElementById('temperature').textContent = d.temperature.toFixed(1) + ' C';
          document.getElementById('duty').textContent        = d.duty + ' %';
          document.getElementById('rpm').textContent         = d.rpm + ' RPM';
          const manual = d.manual;
          document.getElementById('btn-auto').style.background   = manual ? '#90a4ae' : '#4caf50';
          document.getElementById('btn-manual').style.background = manual ? '#f44336' : '#90a4ae';
          document.getElementById('manual-controls').style.display = manual ? 'block' : 'none';
        })
        .catch(() => { document.getElementById('status-dot').style.background = '#f44336'; });
    }
    setInterval(refreshStatus, 2000);
    refreshStatus();
  </script>
</body>
</html>
)rawhtml";

// ---------------------------------------------------------------------------
// Template processor — fills %PLACEHOLDERS% on first page load
// ---------------------------------------------------------------------------
bool pageProcessor(Print& out, const char* param) {
  if (strcmp(param, "HUMIDITY") == 0) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%.1f %%", g_humidity);
    out.print(buf);
    return true;
  }
  if (strcmp(param, "TEMPERATURE") == 0) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%.1f C", g_temperature);
    out.print(buf);
    return true;
  }
  if (strcmp(param, "DUTY") == 0) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%d %%", g_dutyCycle);
    out.print(buf);
    return true;
  }
  if (strcmp(param, "RPM") == 0) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", g_fanRPM);
    out.print(buf);
    return true;
  }
  // Button highlight colors
  if (strcmp(param, "AUTO_ACTIVE") == 0) {
    out.print(g_manualOverride ? "#90a4ae" : "#4caf50");
    return true;
  }
  if (strcmp(param, "MANUAL_ACTIVE") == 0) {
    out.print(g_manualOverride ? "#f44336" : "#90a4ae");
    return true;
  }
  if (strcmp(param, "MANUAL_DISPLAY") == 0) {
    out.print(g_manualOverride ? "block" : "none");
    return true;
  }
  return false;
}

// ---------------------------------------------------------------------------
// WiFi (same pattern as Christmas-PIO)
// ---------------------------------------------------------------------------
void Connect_to_Wifi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  WiFi.setHostname(HOSTNAME);
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

// ---------------------------------------------------------------------------
// Server setup — call from setup()
// ---------------------------------------------------------------------------
void setupWebServer() {
  Connect_to_Wifi();

  server.listen(80);

  // Root page
  server.on("/", HTTP_GET, [](PsychicRequest* request) {
    PsychicStreamResponse response(request, "text/html");
    response.beginSend();
    TemplatePrinter printer(response, pageProcessor);
    printer.print(INDEX_HTML);
    printer.flush();
    return response.endSend();
  });

  // Live status JSON — polled by JS every 2s
  server.on("/status", HTTP_GET, [](PsychicRequest* request) {
    char json[128];
    snprintf(json, sizeof(json),
      "{\"humidity\":%.1f,\"temperature\":%.1f,\"duty\":%d,\"rpm\":%d,\"manual\":%s}",
      g_humidity, g_temperature, g_dutyCycle, g_fanRPM,
      g_manualOverride ? "true" : "false");
    return request->reply(200, "application/json", json);
  });

  // Control endpoint — GET /update?mode=auto|manual  or  /update?duty=0..100
  server.on("/update", HTTP_GET, [](PsychicRequest* request) {
    if (request->hasParam("mode")) {
      String mode = request->getParam("mode")->value();
      g_manualOverride = (mode == "manual");
      Serial.println("Mode: " + mode);
    }
    if (request->hasParam("duty")) {
      int duty = request->getParam("duty")->value().toInt();
      duty = constrain(duty, 0, 100);
      g_manualDuty     = duty;
      g_manualOverride = true;
      Serial.printf("Manual duty: %d%%\n", duty);
    }
    return request->reply(200, "text/plain", "OK");
  });

  Serial.println("Web server started");
}

// ---------------------------------------------------------------------------
// In main.cpp loop(), apply manual override if active, e.g.:
//
//   if (g_manualOverride) {
//     emc2101.setDutyCycle(g_manualDuty);
//     g_dutyCycle = g_manualDuty;
//   } else {
//     g_dutyCycle = humidityToDutyCycle(g_humidity);
//     emc2101.setDutyCycle(g_dutyCycle);
//   }
// ---------------------------------------------------------------------------
