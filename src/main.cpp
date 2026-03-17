#include <Adafruit_EMC2101.h>
#include <Adafruit_SHT31.h>
#include <Arduino.h>
#include <WiFi.h>
#include <PsychicHttp.h>
#include "arduino-secrets.h"
#include "string_constants.h"

Adafruit_EMC2101 emc2101;
Adafruit_SHT31 sht30;
#define BUTTON_PIN  15    // GPIO connected to button NO terminal
#define LED_PIN     12    // GPIO driving NPN transistor base
#define HOSTNAME "bathroom-fan"

// Humidity thresholds — hysteresis band prevents flutter at the boundary
static const float HUMIDITY_OFF    = 48.0;  // turn off below this
static const float HUMIDITY_ON     = 52.0;  // turn on above this
static const float HUMIDITY_FULL   = 85.0;  // above this, fan is at 100%

// Fan duty cycle limits (percent)
static const uint8_t FAN_MIN_DUTY  = 25;    // minimum to keep fan spinning when on
static const uint8_t FAN_MAX_DUTY  = 100;

static const unsigned long UPDATE_INTERVAL_MS = 2000;
static const unsigned long DEBOUNCE_MS = 50;
static unsigned long lastPressTime  = 0;
static bool          lastButtonState = HIGH;  // HIGH = not pressed (INPUT_PULLUP)

PsychicHttpServer server;

float   g_humidity    = 0.0f;      // latest humidity reading (%)
float   g_temperature = 0.0f;      // latest temperature reading (C)
int     g_fanRPM      = 0;         // latest fan RPM
int     g_dutyCycle   = 0;         // current duty cycle being applied (0-100)
bool    g_manualOverride = false;  // Manually overriding?
int     g_manualDuty = 0;          // Manually set duty cycle 


uint8_t humidityToDutyCycle(float humidity) {
  static bool fanRunning = false;

  // Hysteresis: only start above HUMIDITY_ON, only stop below HUMIDITY_OFF
  if (fanRunning) {
    if (humidity < HUMIDITY_OFF) fanRunning = false;
  } else {
    if (humidity > HUMIDITY_ON)  fanRunning = true;
  }

  if (!fanRunning)               return 0;
  if (humidity >= HUMIDITY_FULL) return FAN_MAX_DUTY;

  float fraction = (humidity - HUMIDITY_ON) / (HUMIDITY_FULL - HUMIDITY_ON);
  return (uint8_t)(FAN_MIN_DUTY + fraction * (FAN_MAX_DUTY - FAN_MIN_DUTY));
}

void setupButton() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
}

void handleButton() {
  bool currentState = digitalRead(BUTTON_PIN);
  unsigned long now = millis();

  // Detect falling edge (press) with debounce
  if (lastButtonState == HIGH && currentState == LOW) {
    if (now - lastPressTime > DEBOUNCE_MS) {
      lastPressTime = now;

      if (g_manualOverride) {
        g_manualOverride = false;
        Serial.println("Button: auto mode");
      } else {
        g_manualOverride = true;
        g_manualDuty     = 100;
        g_dutyCycle      = 100;           // update immediately so /status is accurate
        emc2101.setDutyCycle(100);        // apply to hardware immediately, don't wait for loop tick
        Serial.println("Button: manual override ON (100%)");
      }
    }
  }
  lastButtonState = currentState;

  // LED mirrors fan running state — use manual duty immediately, don't wait for sensor tick
  int effectiveDuty = g_manualOverride ? g_manualDuty : g_dutyCycle;
  digitalWrite(LED_PIN, effectiveDuty > 0 ? HIGH : LOW);
}


// ---------------------------------------------------------------------------
// Template processor — fills %PLACEHOLDERS% in INDEX_HTML
// ---------------------------------------------------------------------------
static String fillTemplate() {
  String html(INDEX_HTML);
  char buf[32];
  snprintf(buf, sizeof(buf), "%.1f %%", g_humidity);
  html.replace("%HUMIDITY%", buf);
  snprintf(buf, sizeof(buf), "%.1f F", g_temperature * 9.0f / 5.0f + 32.0f);
  html.replace("%TEMPERATURE%", buf);
  snprintf(buf, sizeof(buf), "%d %%", g_dutyCycle);
  html.replace("%DUTY%", buf);
  snprintf(buf, sizeof(buf), "%d", g_fanRPM);
  html.replace("%RPM%", buf);
  html.replace("%AUTO_ACTIVE%",   g_manualOverride ? "#90a4ae" : "#4caf50");
  html.replace("%MANUAL_ACTIVE%", g_manualOverride ? "#f44336" : "#90a4ae");
  html.replace("%MANUAL_DISPLAY%", g_manualOverride ? "block"  : "none");
  return html;
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

  server.begin();

  // Root page
  server.on("/", HTTP_GET, [](PsychicRequest* request, PsychicResponse* response) {
    String html = fillTemplate();
    return response->send(200, "text/html", html.c_str());
  });

  // Live status JSON — polled by JS every 2s
  server.on("/status", HTTP_GET, [](PsychicRequest* request, PsychicResponse* response) {
    char json[128];
    snprintf(json, sizeof(json),
      "{\"humidity\":%.1f,\"temperature\":%.1f,\"duty\":%d,\"rpm\":%d,\"manual\":%s}",
      g_humidity, g_temperature * 9.0f / 5.0f + 32.0f, g_dutyCycle, g_fanRPM,
      g_manualOverride ? "true" : "false");
    return response->send(200, "application/json", json);
  });

  // Control endpoint — GET /update?mode=auto|manual  or  /update?duty=0..100
  server.on("/update", HTTP_GET, [](PsychicRequest* request, PsychicResponse* response) {
    if (request->hasParam("mode")) {
      String mode = request->getParam("mode")->value();
      g_manualOverride = (mode == "manual");
      Serial.println("Mode: " + mode);
    }
    if (request->hasParam("duty")) {
      int duty = request->getParam("duty")->value().toInt();
      duty = constrain(duty, 0, 100);
      g_manualDuty     = duty;
      g_dutyCycle      = duty;
      g_manualOverride = true;
      Serial.printf("Manual duty: %d%%\n", duty);
    }
    return response->send(200, "text/plain", "OK");
  });

  Serial.println("Web server started");
}

void setup() {
  Serial.begin(115200);

  Serial.println("Bathroom Fan Controller");

  if (!emc2101.begin()) {
    Serial.println("ERROR: EMC2101 not found");
    while (1) delay(10);
  }
  Serial.println("EMC2101 OK");

  if (!sht30.begin(0x44)) {
    Serial.println("ERROR: SHT30 not found");
    while (1) delay(10);
  }
  Serial.println("SHT30 OK");

  emc2101.enableTachInput(true);
  emc2101.setPWMDivisor(0);
  emc2101.setDutyCycle(0);
  setupButton(); // Setup the Pushbutton
  setupWebServer();  
}

void loop() {
  static unsigned long lastUpdate = 0;
  unsigned long now = millis();
  handleButton(); //Handle the Pushbutton

  if (now - lastUpdate < UPDATE_INTERVAL_MS) return;
  lastUpdate = now;

  g_humidity = sht30.readHumidity();
  g_temperature = sht30.readTemperature();

  if (isnan(g_humidity) || isnan(g_temperature)) {
    Serial.println("ERROR: SHT30 read failed");
    return;
  }

  
  g_dutyCycle = g_manualOverride ? (uint8_t)g_manualDuty : humidityToDutyCycle(g_humidity);
  emc2101.setDutyCycle(g_dutyCycle);
  g_fanRPM = emc2101.getFanRPM();

  Serial.printf("Humidity: %.1f%%  Temp: %.1fC (%.1fF)  Fan: %d%%  RPM: %d\n",
                g_humidity, g_temperature, g_temperature * 9.0f / 5.0f + 32.0f, g_dutyCycle, g_fanRPM);
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
