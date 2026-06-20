/*
 * =====================================================================
 *  AEROSPACE LIPID VESICLE PAYLOAD DATA LOGGER
 *  Rocket Flight Sensor Acquisition System
 * =====================================================================
 *
 *  Purpose : Log accelerometer, thermometer, and pressure data
 *            during rocket flight for a lipid vesicle payload study.
 *  Target  : Arduino Nano (ATmega328P)
 *  Sensors : MPU-6050 (IMU), DS18B20 (temperature), BMP388 (pressure)
 *  Storage : microSD card (CSV format)
 *  Rates   : IMU @ 100 Hz, Thermal @ 1 Hz, Pressure @ 1 Hz
 *
 *  Author  : [Your Name]
 *  Date    : [Current Date]
 *  Version : 1.0
 *
 *  See README.md, PINOUT.md, and DATA_FORMAT.md for full details.
 * =====================================================================
 */

// ---------------------------------------------------------------------
//  LIBRARIES
// ---------------------------------------------------------------------
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// ---------------------------------------------------------------------
//  PIN DEFINITIONS
// ---------------------------------------------------------------------
#define CS_PIN        10   // microSD card chip select (SPI)
#define IMU_INT_PIN    2   // MPU-6050 interrupt pin (optional)
#define THERM_PIN      5   // DS18B20 thermometer data pin (OneWire)
#define STATUS_LED    13   // Status indicator LED (onboard)

// ---------------------------------------------------------------------
//  SENSOR I2C ADDRESSES & REGISTERS
// ---------------------------------------------------------------------
#define MPU6050_ADDR   0x68    // MPU-6050 I2C address
#define BMP388_ADDR    0x77    // BMP388 I2C address

// MPU-6050 register map
#define PWR_MGMT_1     0x6B
#define ACCEL_XOUT_H   0x3B
#define GYRO_XOUT_H    0x43
#define ACCEL_CONFIG   0x1C

// ---------------------------------------------------------------------
//  SAMPLING INTERVALS (milliseconds)
// ---------------------------------------------------------------------
#define IMU_INTERVAL_MS       10     // 100 Hz
#define THERMAL_INTERVAL_MS   1000   // 1 Hz
#define PRESSURE_INTERVAL_MS  1000   // 1 Hz

// ---------------------------------------------------------------------
//  DATA STRUCTURES
// ---------------------------------------------------------------------
struct IMUData {
  int16_t accelX, accelY, accelZ;
  int16_t gyroX,  gyroY,  gyroZ;
  float   accelX_g, accelY_g, accelZ_g;   // accelerometer in g
};

struct ThermalData {
  float temperature_C;
};

struct PressureData {
  float pressure_mbar;
  float altitude_m;
};

// ---------------------------------------------------------------------
//  GLOBAL STATE
// ---------------------------------------------------------------------
File          dataFile;
unsigned long lastIMUtime      = 0;
unsigned long lastThermalTime  = 0;
unsigned long lastPressureTime = 0;
unsigned long flightStartTime  = 0;
bool          logging_active   = false;

// OneWire thermometer setup
OneWire           oneWire(THERM_PIN);
DallasTemperature sensors(&oneWire);

// ---------------------------------------------------------------------
//  FORWARD DECLARATIONS
// ---------------------------------------------------------------------
bool        initializeMPU6050();
bool        initializeBMP388();
IMUData     readMPU6050();
ThermalData readDS18B20();
PressureData readBMP388();
void        createDataFile();
void        logIMUData(unsigned long elapsed_ms, IMUData imu);
void        logThermalData(unsigned long elapsed_ms, ThermalData thermal);
void        logPressureData(unsigned long elapsed_ms, PressureData pressure);
void        blink_error();


// =====================================================================
//  SETUP
// =====================================================================
void setup() {
  // ---- Serial ----
  Serial.begin(115200);
  delay(500);
  Serial.println(F("\n\n=== AEROSPACE LIPID VESICLE PAYLOAD DATA LOGGER ==="));
  Serial.println(F("Initializing sensors..."));

  // ---- Status LED ----
  pinMode(STATUS_LED, OUTPUT);
  digitalWrite(STATUS_LED, LOW);

  // ---- I2C bus ----
  Wire.begin();
  delay(100);

  // ---- microSD card ----
  if (!SD.begin(CS_PIN)) {
    Serial.println(F("ERROR: microSD card initialization failed!"));
    Serial.println(F("Check: CS pin (10), SPI connections, card presence"));
    while (1) { blink_error(); }
  }
  Serial.println(F("[OK] microSD card initialized"));

  // ---- MPU-6050 ----
  if (!initializeMPU6050()) {
    Serial.println(F("ERROR: MPU-6050 initialization failed!"));
    Serial.println(F("Check: I2C address (0x68), Wire connections"));
    while (1) { blink_error(); }
  }
  Serial.println(F("[OK] MPU-6050 IMU initialized"));

  // ---- DS18B20 ----
  sensors.begin();
  if (sensors.getDeviceCount() == 0) {
    Serial.println(F("WARNING: No DS18B20 thermometer detected"));
    Serial.println(F("Check: Pin 5, OneWire connections"));
  } else {
    Serial.print(F("[OK] DS18B20 thermometer initialized ("));
    Serial.print(sensors.getDeviceCount());
    Serial.println(F(" sensor(s) found)"));
  }

  // ---- BMP388 ----
  if (!initializeBMP388()) {
    Serial.println(F("WARNING: BMP388 pressure sensor not responding"));
    Serial.println(F("Check: I2C address (0x77), Wire connections"));
  } else {
    Serial.println(F("[OK] BMP388 pressure sensor initialized"));
  }

  // ---- Create log file ----
  createDataFile();

  // ---- Begin logging ----
  flightStartTime = millis();
  logging_active  = true;
  Serial.println(F("\n*** LOGGING ACTIVE ***"));
  Serial.println(F("Sensors running at 100 Hz (IMU), 1 Hz (Thermal), 1 Hz (Pressure)"));
  Serial.println(F("Press any key in serial monitor to stop logging\n"));

  digitalWrite(STATUS_LED, HIGH);
}


// =====================================================================
//  MAIN LOOP
// =====================================================================
void loop() {
  unsigned long currentTime = millis();
  unsigned long elapsed_ms  = currentTime - flightStartTime;

  // ---- Serial stop command ----
  if (Serial.available()) {
    Serial.read();
    logging_active = false;
    dataFile.close();
    Serial.println(F("\n*** LOGGING STOPPED ***"));
    digitalWrite(STATUS_LED, LOW);
    while (1);
  }

  // ---- IMU log (100 Hz) ----
  if (currentTime - lastIMUtime >= IMU_INTERVAL_MS) {
    lastIMUtime = currentTime;

    IMUData imu = readMPU6050();

    // Convert raw values to g units (configured ±16g range)
    imu.accelX_g = imu.accelX / 2048.0f;
    imu.accelY_g = imu.accelY / 2048.0f;
    imu.accelZ_g = imu.accelZ / 2048.0f;

    logIMUData(elapsed_ms, imu);
  }

  // ---- Thermal log (1 Hz) ----
  if (currentTime - lastThermalTime >= THERMAL_INTERVAL_MS) {
    lastThermalTime = currentTime;

    ThermalData thermal = readDS18B20();
    logThermalData(elapsed_ms, thermal);
  }

  // ---- Pressure log (1 Hz) ----
  if (currentTime - lastPressureTime >= PRESSURE_INTERVAL_MS) {
    lastPressureTime = currentTime;

    PressureData pressure = readBMP388();
    logPressureData(elapsed_ms, pressure);
  }

  // ---- Status LED heartbeat (every 500 ms) ----
  digitalWrite(STATUS_LED, ((elapsed_ms / 500) % 2 == 0) ? HIGH : LOW);
}


// =====================================================================
//  SENSOR INITIALIZATION
// =====================================================================

bool initializeMPU6050() {
  // Wake up MPU-6050
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(PWR_MGMT_1);
  Wire.write(0x00);                 // clear sleep bit
  Wire.endTransmission();
  delay(100);

  // Set accelerometer range to ±16g
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(ACCEL_CONFIG);
  Wire.write(0x18);                 // 0x18 = ±16g range
  Wire.endTransmission();

  // TODO: read back PWR_MGMT_1 / ACCEL_CONFIG to verify the write
  return true;
}

bool initializeBMP388() {
  // Minimal presence check (full BMP388 driver not implemented yet)
  Wire.beginTransmission(BMP388_ADDR);
  return (Wire.endTransmission() == 0);
}


// =====================================================================
//  SENSOR READ FUNCTIONS
// =====================================================================

IMUData readMPU6050() {
  IMUData imu;

  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(ACCEL_XOUT_H);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU6050_ADDR, 14, true);   // 6 accel + 2 temp + 6 gyro

  imu.accelX = (Wire.read() << 8) | Wire.read();
  imu.accelY = (Wire.read() << 8) | Wire.read();
  imu.accelZ = (Wire.read() << 8) | Wire.read();

  Wire.read();   // skip MPU temperature MSB
  Wire.read();   // skip MPU temperature LSB

  imu.gyroX  = (Wire.read() << 8) | Wire.read();
  imu.gyroY  = (Wire.read() << 8) | Wire.read();
  imu.gyroZ  = (Wire.read() << 8) | Wire.read();

  return imu;
}

ThermalData readDS18B20() {
  ThermalData thermal;
  sensors.requestTemperatures();
  thermal.temperature_C = sensors.getTempCByIndex(0);
  return thermal;
}

PressureData readBMP388() {
  PressureData pressure;
  // TODO: replace placeholder with full BMP388 calibrated read
  //       (use Adafruit_BMP3XX library or implement compensation directly)
  pressure.pressure_mbar = 1013.25f;
  pressure.altitude_m    = 0.0f;
  return pressure;
}


// =====================================================================
//  DATA LOGGING
// =====================================================================

void createDataFile() {
  // TODO: replace millis()-based name with RTC timestamp once added
  String filename = "FLIGHT_" + String(millis()) + ".csv";

  dataFile = SD.open(filename, FILE_WRITE);
  if (!dataFile) {
    Serial.println(F("ERROR: Could not create data file!"));
    return;
  }

  dataFile.println(F("Time_ms,AccelX_g,AccelY_g,AccelZ_g,"
                     "GyroX_dps,GyroY_dps,GyroZ_dps,"
                     "Temp_C,Pressure_mbar,Altitude_m"));
  dataFile.flush();

  Serial.print(F("Data file created: "));
  Serial.println(filename);
}

void logIMUData(unsigned long elapsed_ms, IMUData imu) {
  if (!dataFile) return;

  dataFile.print(elapsed_ms);            dataFile.print(',');
  dataFile.print(imu.accelX_g, 4);       dataFile.print(',');
  dataFile.print(imu.accelY_g, 4);       dataFile.print(',');
  dataFile.print(imu.accelZ_g, 4);       dataFile.print(',');
  dataFile.print(imu.gyroX * 250.0f / 32768.0f, 2); dataFile.print(',');
  dataFile.print(imu.gyroY * 250.0f / 32768.0f, 2); dataFile.print(',');
  dataFile.print(imu.gyroZ * 250.0f / 32768.0f, 2);
  dataFile.print(F(",,,"));              // temp / pressure / altitude blank at 100 Hz
  dataFile.println();

  dataFile.flush();
}

void logThermalData(unsigned long elapsed_ms, ThermalData thermal) {
  if (!dataFile) return;

  dataFile.print(elapsed_ms);
  dataFile.print(F(",,,,,,,"));          // 6 empty IMU columns
  dataFile.print(thermal.temperature_C, 2);
  dataFile.print(F(",,"));
  dataFile.println();

  dataFile.flush();
}

void logPressureData(unsigned long elapsed_ms, PressureData pressure) {
  if (!dataFile) return;

  dataFile.print(elapsed_ms);
  dataFile.print(F(",,,,,,,"));          // 6 empty IMU columns
  dataFile.print(',');                   // empty temp column
  dataFile.print(pressure.pressure_mbar, 2);
  dataFile.print(',');
  dataFile.print(pressure.altitude_m, 1);
  dataFile.println();

  dataFile.flush();
}


// =====================================================================
//  UTILITY FUNCTIONS
// =====================================================================

void blink_error() {
  digitalWrite(STATUS_LED, HIGH);
  delay(100);
  digitalWrite(STATUS_LED, LOW);
  delay(100);
}
