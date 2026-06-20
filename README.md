# Aerospace Lipid Vesicle Payload Data Logger

A rocket-flight sensor acquisition system that logs accelerometer, gyroscope, temperature, and barometric pressure data to a microSD card for a lipid vesicle payload experiment.

The flight computer is an **Arduino Nano**, and the firmware writes a CSV log file at:

- **100 Hz** for the 6-axis IMU (MPU-6050)
- **1 Hz** for the high-precision thermometer (DS18B20)
- **1 Hz** for the barometric pressure sensor (BMP388)

---

## Table of Contents

- [Project Overview](#project-overview)
- [Hardware Requirements](#hardware-requirements)
- [Wiring](#wiring)
- [Library Dependencies](#library-dependencies)
- [Uploading the Firmware](#uploading-the-firmware)
- [CSV Output Format](#csv-output-format)
- [Operating Procedure](#operating-procedure)
- [Status LED Codes](#status-led-codes)
- [Repository Layout](#repository-layout)
- [Roadmap / TODO](#roadmap--todo)

---

## Project Overview

This payload is part of a study on how lipid vesicle stability is affected by the mechanical and thermal environment of a sounding-rocket flight. To correlate post-flight vesicle analysis with in-flight conditions, the data logger captures:

| Quantity            | Sensor    | Rate    | Range / Resolution           |
|---------------------|-----------|---------|------------------------------|
| 3-axis acceleration | MPU-6050  | 100 Hz  | ±20 g, 16-bit                |
| 3-axis angular rate | MPU-6050  | 100 Hz  | ±250 °/s, 16-bit             |
| Temperature         | DS18B20   | 1 Hz    | -55 to +125 °C, 12-bit       |
| Pressure / altitude | BMP388    | 1 Hz    | 300–1250 hPa                 |

Data is written to a CSV file on a microSD card (`FLIGHT_<millis>.csv`).

---

## Hardware Requirements

| Part                              | Qty | Notes                                          |
|-----------------------------------|-----|------------------------------------------------|
| Arduino Nano (ATmega328P)         | 1   | 5 V logic, USB programming                     |
| MPU-6050 IMU breakout             | 1   | I2C, 3.3 V – 5 V tolerant                      |
| DS18B20 digital thermometer       | 1   | OneWire, waterproof variant recommended        |
| 4.7 kΩ resistor                   | 1   | Pull-up between DS18B20 DATA and VCC           |
| BMP388 barometer breakout         | 1   | Adafruit/SparkFun module, I2C address 0x77     |
| microSD card module (SPI)         | 1   | 5 V level-shifted, FAT32-formatted SD card     |
| microSD card                      | 1   | 8–32 GB, Class 10                              |
| LiPo / 9 V battery + regulator    | 1   | Powering Nano via VIN (7–12 V)                 |
| Status LED                        | 1   | Uses onboard LED on D13 by default             |
| Prototype board / PCB             | 1   | Mounted inside payload bay                     |

> **Power note:** All sensors share the Nano's 3.3 V or 5 V rail. Confirm that your breakout boards are compatible with the rail you connect to.

---

## Wiring

For complete pin assignments, see **[PINOUT.md](PINOUT.md)**. Quick summary:

```
Arduino Nano  <-->  Sensor / Module
---------------------------------------------------------
A4 (SDA)       <->  MPU-6050 SDA, BMP388 SDA
A5 (SCL)       <->  MPU-6050 SCL, BMP388 SCL
D2             <->  MPU-6050 INT (optional)
D5             <->  DS18B20 DATA (with 4.7 kΩ pull-up to VCC)
D10 (SS)       <->  microSD CS
D11 (MOSI)     <->  microSD MOSI
D12 (MISO)     <->  microSD MISO
D13 (SCK)      <->  microSD SCK  + Status LED (onboard)
3.3 V / 5 V    <->  Sensor VCC rails
GND            <->  Common ground
```

**I2C addresses**

| Device   | Address |
|----------|---------|
| MPU-6050 | 0x68    |
| BMP388   | 0x77    |

---

## Library Dependencies

Install through the Arduino IDE: **Sketch → Include Library → Manage Libraries…**

| Library                     | Author             | Tested Version | Purpose                       |
|-----------------------------|--------------------|----------------|-------------------------------|
| `Wire`                      | Arduino (built-in) | bundled        | I2C bus                       |
| `SPI`                       | Arduino (built-in) | bundled        | SPI bus                       |
| `SD`                        | Arduino (built-in) | bundled        | microSD card filesystem       |
| `OneWire`                   | Jim Studt          | 2.3.7+         | OneWire protocol for DS18B20  |
| `DallasTemperature`         | Miles Burton       | 3.11.0+        | DS18B20 driver                |
| `Adafruit BMP3XX Library`   | Adafruit           | 2.1.5+         | BMP388 driver (planned)       |
| `MPU6050` (Electronic Cats) | Electronic Cats    | 1.4.1+         | Optional drop-in IMU driver   |

A machine-readable list is available in **[DEPENDENCIES.txt](DEPENDENCIES.txt)**.

---

## Uploading the Firmware

1. Clone or download this repository.
2. Open `payload_arduino.ino` in the **Arduino IDE** (1.8.x or 2.x).
3. Install the libraries listed above.
4. Select **Tools → Board → Arduino Nano**.
5. Select the correct processor (usually **ATmega328P**, or **ATmega328P (Old Bootloader)** for clones).
6. Select the correct COM port under **Tools → Port**.
7. Click **Upload**.
8. Open the **Serial Monitor** at **115200 baud** to verify the boot banner and sensor-init messages.

---

## CSV Output Format

The firmware writes one CSV file per power cycle:

```
FLIGHT_<millis>.csv
```

Header:

```
Time_ms,AccelX_g,AccelY_g,AccelZ_g,GyroX_dps,GyroY_dps,GyroZ_dps,Temp_C,Pressure_mbar,Altitude_m
```

| Column          | Unit  | Source     |
|-----------------|-------|------------|
| `Time_ms`       | ms    | `millis()` since boot |
| `AccelX/Y/Z_g`  | g     | MPU-6050 (±16 g) |
| `GyroX/Y/Z_dps` | °/s   | MPU-6050 (±250 °/s) |
| `Temp_C`        | °C    | DS18B20 |
| `Pressure_mbar` | mbar  | BMP388 |
| `Altitude_m`    | m     | BMP388 (computed) |

Because the IMU runs at 100 Hz while the thermal and pressure sensors run at 1 Hz, most rows contain blanks in the slower columns. See **[DATA_FORMAT.md](DATA_FORMAT.md)** for sample rows, units, and expected ranges.

---

## Operating Procedure

1. Insert a FAT32-formatted microSD card.
2. Power the Nano via USB or external battery.
3. Watch the onboard LED:
   - **Solid on, then heartbeat** → logging started.
   - **Rapid blink** → fatal init error (microSD or IMU). Check wiring.
4. To stop logging, open the Serial Monitor and send any character. The active file is closed cleanly before the program halts.
5. Remove the SD card and read the CSV on a host computer.

---

## Status LED Codes

| Pattern                         | Meaning                                  |
|---------------------------------|------------------------------------------|
| Off                             | Pre-init or stopped                      |
| Solid HIGH at end of `setup()`  | Logging just started                     |
| 0.5 s on / 0.5 s off heartbeat  | Normal logging in progress               |
| ~10 Hz rapid blink              | Fatal error — execution halted           |

---

## Repository Layout

```
payload arduino/
├── payload_arduino.ino   # Main firmware sketch
├── README.md             # This file
├── PINOUT.md             # Pin and bus assignments
├── DATA_FORMAT.md        # CSV column reference
└── DEPENDENCIES.txt      # Required Arduino libraries
```

---

## Roadmap / TODO

- [ ] Full BMP388 driver integration (calibrated pressure + altitude).
- [ ] Add RTC (DS3231) for absolute timestamps in filenames and rows.
- [ ] Verify MPU-6050 register write-backs in `initializeMPU6050()`.
- [ ] Switch SD logging to a single unified row schema with interpolation/hold.
- [ ] Add launch-detect trigger (acceleration threshold) to avoid pre-flight log noise.
- [ ] Optional onboard buzzer for post-recovery location.

---
