# Bathroom Fan Controller - Project Notes

## Hardware

### Microcontroller
- **Adafruit Feather ESP32 V2**
- PlatformIO board: `adafruit_feather_esp32_v2`

### Fan Controller
- **Adafruit EMC2101** — controls fan PWM and reads tach (RPM)
- Interface: I2C
- Library: `adafruit/Adafruit EMC2101@^1.0.7`

### Humidity Sensor
- **Adafruit SHT30 Weatherproof Sensor** (product #4099)
  - Sealed stainless mesh housing, suited for bathroom/steam environment
  - ~1m cable, 4 wires
  - Interface: I2C, fixed address `0x44`
  - Accuracy: ±2% RH, ±0.5°C
  - Operates at 3–5VDC
- Library: `adafruit/Adafruit SHT31 Library@^2.2.2` (covers full SHT3x family)

## Wiring: SHT30 to Feather ESP32 V2

| SHT30 Wire       | Feather Pin     |
|------------------|-----------------|
| Red/Brown (VCC)  | 3V              |
| Black (GND)      | GND             |
| Yellow (SCL)     | SCL (GPIO 20)   |
| Green/Blue (SDA) | SDA (GPIO 22)   |

> **Note:** Some units have SDA/SCL swapped at the factory. If the sensor doesn't respond, try swapping the yellow and green/blue wires.

Both the EMC2101 and SHT30 share the same I2C bus — no conflicts.

## Fan Control Logic

**Proportional humidity control** — fan speed scales linearly with humidity.

| Humidity       | Fan Duty Cycle         |
|----------------|------------------------|
| Below 55% RH   | Off (0%)               |
| 55% – 85% RH   | 25% – 100% (linear)    |
| Above 85% RH   | Full speed (100%)      |

- Minimum duty cycle of 25% when fan is running (prevents stall at low speeds)
- Update interval: 2000ms (SHT30 minimum reliable interval is ~1s)

Tunable constants in code:
- `HUMIDITY_OFF` — threshold below which fan is off
- `HUMIDITY_FULL` — threshold at which fan hits 100%
- `FAN_MIN_DUTY` — minimum duty cycle to keep fan spinning (depends on specific fan)
- `UPDATE_INTERVAL_MS` — how often to read sensor and update fan

## Notes on Types
- `uint8_t` = unsigned 8-bit integer (0–255). Used for duty cycle values in the suggested code.
- `int` works equally well for this application; `uint8_t` just makes intent explicit in embedded contexts.

## Files
- `src/main.cpp` — current main (basic EMC2101 demo)
- `src/main.cpp.suggested` — proportional humidity control implementation (pending review)
- `platformio.ini` — PlatformIO config with both library dependencies
