# Bathroom Fan Controller - Project Notes

## Hardware

### Microcontroller
- **Adafruit Feather ESP32 V2**
- PlatformIO board: `adafruit_feather_esp32_v2`

### Power Supply

- **12V DC power supply** — powers the fan directly
- **UBEC DC/DC Step-Down Buck Converter** (Adafruit #1385) — converts 12V → 5V for the Feather
  - Input: 6–16V, Output: 5V @ 3A continuous

#### Wiring: 12V → Feather via Buck Converter

| Buck Converter | Connect to        |
|----------------|-------------------|
| IN+            | 12V supply +      |
| IN−            | 12V supply GND    |
| OUT+           | Feather `USB` pin |
| OUT−           | Feather `GND`     |

> **Important:** Feed 5V into the Feather's `USB` pin only. Do NOT use `BAT` (expects 3.7–4.2V) or `3V` (bypasses regulator).

> All GNDs (12V supply, buck converter, Feather, EMC2101, fan) must be tied to a **common ground**.

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

### Physical Button + LED Indicator

- **Adafruit #481** — Rugged Metal Pushbutton with Blue LED Ring (16mm, momentary)
  - Button: momentary, COM → GND, NO → GPIO (INPUT_PULLUP)
  - LED: ~20mA, rated 6V with built-in resistor; runs slightly dim from 5V rail
- **PN2222 NPN Transistor** (Adafruit #756, 10-pack) — switches LED from 5V rail
  - ESP32 GPIO can only source ~12mA; LED needs 20mA at 5–6V, so a transistor is required
  - GPIO controls the base (~2mA); LED current flows from 5V rail through transistor

#### Wiring: Button LED Circuit

```
Feather USB pin (5V) ──── LED+
                          LED− ──── PN2222 collector
                          PN2222 emitter ──── GND
                          PN2222 base ──── 1kΩ ──── GPIO 14
```

Button wiring: COM → GND, NO → GPIO 15 (INPUT_PULLUP, no external resistor needed)

> The Feather `USB` pin exposes the 5V rail from the buck converter — tap here for LED power.

#### Behavior

- Press toggles manual override on/off; entering manual forces fan to 100%
- LED is ON whenever fan duty cycle > 0
- Button and web server share the same `g_manualOverride` flag — either can cancel the other

See `src/pushbutton_suggestion.cpp` for implementation.

## Wiring: EMC2101 to Feather ESP32 V2 (I2C)

| EMC2101 Pin | Feather Pin   |
|-------------|---------------|
| VIN         | 3V            |
| GND         | GND           |
| SCL         | SCL (GPIO 20) |
| SDA         | SDA (GPIO 22) |

## Wiring: EMC2101 to Fan (4-wire PWM)

| EMC2101 Pin | Fan Wire        |
|-------------|-----------------|
| TACH        | Yellow (tach)   |
| FAN         | Blue (PWM)      |

Fan power connects directly to the power supply, not through the EMC2101:

| Fan Wire   | Connect to       |
|------------|------------------|
| Red (+12V) | 12V power supply |
| Black      | Common GND       |

> **Note:** DP/DN pins are for an external thermal diode — leave unconnected if not used.

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
