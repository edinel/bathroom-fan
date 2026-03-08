#include <Adafruit_EMC2101.h>
#include <Adafruit_SHT31.h>

Adafruit_EMC2101 emc2101;
Adafruit_SHT31 sht30;

// Humidity thresholds
static const float HUMIDITY_OFF    = 55.0;  // below this, fan is off
static const float HUMIDITY_FULL   = 85.0;  // above this, fan is at 100%

// Fan duty cycle limits (percent)
static const uint8_t FAN_MIN_DUTY  = 25;    // minimum to keep fan spinning when on
static const uint8_t FAN_MAX_DUTY  = 100;

static const unsigned long UPDATE_INTERVAL_MS = 2000;

uint8_t humidityToDutyCycle(float humidity) {
  if (humidity <= HUMIDITY_OFF)   return 0;
  if (humidity >= HUMIDITY_FULL)  return FAN_MAX_DUTY;

  float fraction = (humidity - HUMIDITY_OFF) / (HUMIDITY_FULL - HUMIDITY_OFF);
  return (uint8_t)(FAN_MIN_DUTY + fraction * (FAN_MAX_DUTY - FAN_MIN_DUTY));
}

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

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
}

void loop() {
  static unsigned long lastUpdate = 0;
  unsigned long now = millis();

  if (now - lastUpdate < UPDATE_INTERVAL_MS) return;
  lastUpdate = now;

  float humidity    = sht30.readHumidity();
  float temperature = sht30.readTemperature();

  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("ERROR: SHT30 read failed");
    return;
  }

  uint8_t duty = humidityToDutyCycle(humidity);
  emc2101.setDutyCycle(duty);

  Serial.printf("Humidity: %.1f%%  Temp: %.1fC  Fan: %d%%  RPM: %d\n",
                humidity, temperature, duty, emc2101.getFanRPM());
}
