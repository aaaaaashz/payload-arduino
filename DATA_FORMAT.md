# CSV Data Format

The payload firmware writes a single CSV log file per power cycle. This document describes the file naming convention, column meanings, units, expected ranges, and how rows are populated by the three different sampling rates.

---

## File Naming

```
FLIGHT_<millis>.csv
```

`<millis>` is the value of `millis()` at the moment the file is opened (i.e., a few hundred ms after boot). This guarantees a unique filename per power cycle on the same SD card.

> **Future:** once a DS3231 RTC is added, filenames will switch to a wall-clock timestamp such as `FLIGHT_20260619_193245.csv`.

---

## Header

```
Time_ms,AccelX_g,AccelY_g,AccelZ_g,GyroX_dps,GyroY_dps,GyroZ_dps,Temp_C,Pressure_mbar,Altitude_m
```

| # | Column          | Unit | Sensor   | Sample Rate | Expected Range       | Notes |
|---|-----------------|------|----------|-------------|----------------------|-------|
| 1 | `Time_ms`       | ms   | Nano     | every row   | 0 … 2^32 − 1         | Milliseconds since boot |
| 2 | `AccelX_g`      | g    | MPU-6050 | 100 Hz      | −16 … +16            | ±16 g full scale |
| 3 | `AccelY_g`      | g    | MPU-6050 | 100 Hz      | −16 … +16            | |
| 4 | `AccelZ_g`      | g    | MPU-6050 | 100 Hz      | −16 … +16            | ~1 g at rest |
| 5 | `GyroX_dps`     | °/s  | MPU-6050 | 100 Hz      | −250 … +250          | ±250 °/s full scale |
| 6 | `GyroY_dps`     | °/s  | MPU-6050 | 100 Hz      | −250 … +250          | |
| 7 | `GyroZ_dps`     | °/s  | MPU-6050 | 100 Hz      | −250 … +250          | |
| 8 | `Temp_C`        | °C   | DS18B20  | 1 Hz        | −55 … +125 (typ. −40 … +85 in flight) | 0.0625 °C resolution |
| 9 | `Pressure_mbar` | mbar | BMP388   | 1 Hz        | ~300 … ~1100         | Sea level ≈ 1013.25 |
| 10| `Altitude_m`    | m    | BMP388   | 1 Hz        | 0 … ~10,000          | Derived from pressure |

---

## How Rows Are Populated

Because the three sensors run at different rates, **each row contains values for only one sensor group**; the other columns are written as empty fields. Post-processing scripts can re-sample or align the streams.

### IMU row (every 10 ms, 100 Hz)

Columns 2–7 are populated. Columns 8–10 are blank.

```
Time_ms,AccelX_g,AccelY_g,AccelZ_g,GyroX_dps,GyroY_dps,GyroZ_dps,Temp_C,Pressure_mbar,Altitude_m
1234,0.0123,-0.0042,0.9981,-0.12,0.07,0.04,,,
```

### Thermal row (every 1000 ms, 1 Hz)

Only column 8 is populated.

```
1500,,,,,,,21.31,,
```

### Pressure row (every 1000 ms, 1 Hz)

Only columns 9 and 10 are populated.

```
1500,,,,,,,,1012.84,3.6
```

> Result: a typical second of flight contains ~100 IMU rows and 1 thermal + 1 pressure row interleaved.

---

## Sample Excerpt

```
Time_ms,AccelX_g,AccelY_g,AccelZ_g,GyroX_dps,GyroY_dps,GyroZ_dps,Temp_C,Pressure_mbar,Altitude_m
10,0.0034,-0.0021,0.9976,0.04,-0.02,0.01,,,
20,0.0041,-0.0018,0.9981,0.05,-0.01,0.02,,,
30,0.0038,-0.0024,0.9979,0.03,-0.03,0.01,,,
...
1000,,,,,,,21.25,,
1000,,,,,,,,1013.21,0.4
1010,0.0040,-0.0020,0.9982,0.04,-0.02,0.01,,,
```

---

## Suggested Post-Processing

Recommended Python (pandas) recipe:

```python
import pandas as pd

df = pd.read_csv("FLIGHT_1234.csv")

imu      = df.dropna(subset=["AccelX_g"])
thermal  = df.dropna(subset=["Temp_C"])
pressure = df.dropna(subset=["Pressure_mbar"])

# forward-fill slow sensors onto the IMU timeline
merged = imu.set_index("Time_ms").join(
    thermal.set_index("Time_ms")["Temp_C"]
).join(
    pressure.set_index("Time_ms")[["Pressure_mbar", "Altitude_m"]]
).ffill()
```

This yields a unified 100 Hz timeline with carried-forward temperature and pressure for plotting and analysis.

---

## Unit Conversions Reference

| Raw                | Scale used in firmware                        | Result   |
|--------------------|-----------------------------------------------|----------|
| Accel raw / 2048   | for ±16 g range (`ACCEL_CONFIG = 0x18`)       | g        |
| Gyro raw × 250/32768 | for ±250 °/s range (default `GYRO_CONFIG`)  | °/s      |
| DS18B20            | library returns °C directly                   | °C       |
| BMP388 pressure    | sensor returns Pa; firmware writes mbar (Pa × 0.01) | mbar |

---

## Known Limitations (v1.0)

- BMP388 read returns placeholder values until the full calibrated driver is wired in (see README roadmap).
- `Time_ms` resets when power cycles. Without an RTC, multiple log files cannot be cross-correlated by wall-clock time.
- Slow-sensor rows use the IMU's separate row format; tools that require a fully populated grid must re-sample (see snippet above).
