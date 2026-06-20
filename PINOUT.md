# Pinout & Bus Reference

Reference for wiring the Arduino Nano to the MPU-6050, DS18B20, BMP388, and microSD card module used in the payload.

---

## Arduino Nano Pin Assignments

| Nano Pin | Direction | Function           | Connects To                    | Notes |
|----------|-----------|--------------------|--------------------------------|-------|
| **D2**   | IN        | `IMU_INT_PIN`      | MPU-6050 `INT`                 | Optional motion interrupt |
| **D5**   | IN/OUT    | `THERM_PIN`        | DS18B20 `DATA`                 | Needs 4.7 kΩ pull-up to VCC |
| **D10**  | OUT       | `CS_PIN` (SPI SS)  | microSD `CS`                   | Chip select for SD card |
| **D11**  | OUT       | SPI `MOSI`         | microSD `MOSI`                 | Hardware SPI |
| **D12**  | IN        | SPI `MISO`         | microSD `MISO`                 | Hardware SPI |
| **D13**  | OUT       | SPI `SCK` + LED    | microSD `SCK` + onboard LED    | Doubles as `STATUS_LED` |
| **A4**   | I/O       | I2C `SDA`          | MPU-6050 `SDA`, BMP388 `SDA`   | Shared I2C bus |
| **A5**   | I/O       | I2C `SCL`          | MPU-6050 `SCL`, BMP388 `SCL`   | Shared I2C bus |
| **3V3**  | PWR       | 3.3 V rail         | BMP388 `VIN` (if 3.3 V part)   | ~50 mA max from Nano |
| **5V**   | PWR       | 5 V rail           | MPU-6050 `VCC`, DS18B20 `VCC`, microSD `VCC` | Confirm breakout supports 5 V |
| **GND**  | PWR       | Ground             | All sensor GNDs                | Single common ground |
| **VIN**  | PWR       | 7–12 V input       | Flight battery (regulated)     | Powers the Nano in flight |

> The status LED uses the Nano's onboard LED on **D13**, which is shared with the SPI clock line. This is acceptable here because the LED is only driven from the heartbeat once SD initialization is complete.

---

## I2C Bus (SDA = A4, SCL = A5)

| Device   | 7-bit Address | `#define` in firmware |
|----------|---------------|-----------------------|
| MPU-6050 | `0x68`        | `MPU6050_ADDR`        |
| BMP388   | `0x77`        | `BMP388_ADDR`         |

Both sensors share the bus. No external pull-ups are required if the breakouts include them (most do); otherwise add 4.7 kΩ to VCC on SDA and SCL.

---

## SPI Bus (microSD Card)

| SPI Signal | Nano Pin | microSD Module Pin |
|------------|----------|--------------------|
| `SS / CS`  | D10      | `CS`               |
| `MOSI`     | D11      | `MOSI`             |
| `MISO`     | D12      | `MISO`             |
| `SCK`      | D13      | `SCK`              |

Use a 5 V-tolerant microSD breakout with onboard level shifting. The SD card itself must be **FAT32**-formatted.

---

## OneWire Bus (DS18B20)

| Signal | Nano Pin | DS18B20 Pin |
|--------|----------|-------------|
| VCC    | 5V       | `VDD`       |
| DATA   | D5       | `DQ`        |
| GND    | GND      | `GND`       |

A **4.7 kΩ resistor** must be connected between `DQ` and `VDD`. Without this pull-up the bus will not be detected.

---

## Optional MPU-6050 Interrupt

If you wire the MPU-6050's `INT` pin to **D2**, the firmware reserves `IMU_INT_PIN` for an interrupt-driven sampling mode (not enabled in v1.0). Leave it unconnected if you are using only timed polling.

---

## Power Budget (typical)

| Device                | Current at 5 V |
|-----------------------|----------------|
| Arduino Nano          | ~20 mA         |
| MPU-6050              | ~4 mA          |
| DS18B20 (active)      | ~1.5 mA        |
| BMP388                | ~0.5 mA        |
| microSD module (write)| 40–100 mA peak |

Plan your flight battery to provide ~200 mA continuous with margin for SD write bursts.
