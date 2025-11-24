/**
 * BNO055Manager.cpp
 *
 * Implementation of BNO055 sensor management
 */

#include "BNO055Manager.h"
#include "CompassUI.h"

// Calibration stability time (milliseconds)
#define CALIB_STABLE_TIME 3000

// Warning check interval (milliseconds)
#define WARNING_CHECK_INTERVAL 5000

// Poor calibration threshold for warnings
#define WARNING_SYS_THRESHOLD 2
#define WARNING_MAG_THRESHOLD 2

// Heading filter alpha (lower = smoother, higher = more responsive)
#define HEADING_FILTER_ALPHA 0.10f

// NVS namespace
#define NVS_NAMESPACE "bno055cal"

/**
 * Constructor
 */
BNO055Manager::BNO055Manager()
  : bno(55, 0x29, &Wire),
    ui(nullptr),
    state(STATE_NORMAL),
    calibrationLoaded(false),
    lastSys(0), lastGyro(0), lastAccel(0), lastMag(0),
    lastCalibCheckTime(0),
    lastWarningCheckTime(0),
    fullyCalibStartTime(0),
    wasFullyCalibrated(false),
    warningActive(false),
    filteredHeading(0.0f),
    headingInitialized(false) {
}

/**
 * Destructor
 */
BNO055Manager::~BNO055Manager() {
  prefs.end();
}

/**
 * Determine I2C pins based on board type and defines
 */
void BNO055Manager::determineI2CPins() {
  #ifdef extSDA
    sdaPin = extSDA;
  #else
    #if defined(CONFIG_IDF_TARGET_ESP32S3)
      sdaPin = 8;  // ESP32-S3 default
    #else
      sdaPin = 21; // ESP32 default
    #endif
  #endif

  #ifdef extSCL
    sclPin = extSCL;
  #else
    #if defined(CONFIG_IDF_TARGET_ESP32S3)
      sclPin = 9;  // ESP32-S3 default
    #else
      sclPin = 22; // ESP32 default
    #endif
  #endif
}

/**
 * Initialize the BNO055 sensor
 */
bool BNO055Manager::begin() {
  // Determine I2C pins
  determineI2CPins();

  // Initialize I2C
  Wire.begin(sdaPin, sclPin);

  Serial.println("BNO055 Manager starting...");
  Serial.printf("I2C SDA: %d, SCL: %d\n", sdaPin, sclPin);

  // Initialize sensor
  if (!bno.begin()) {
    Serial.println("ERROR: BNO055 not detected!");
    return false;
  }

  delay(100);
  bno.setExtCrystalUse(true);

  Serial.println("BNO055 initialized successfully");

  // Try to load calibration from NVS
  if (hasCalibrationInNVS()) {
    loadCalibrationFromNVS();
    calibrationLoaded = true;
    state = STATE_NORMAL;
    Serial.println("Calibration loaded from NVS");
  } else {
    // No calibration found, start auto-calibration
    calibrationLoaded = false;
    state = STATE_AUTO_CALIBRATING;
    Serial.println("No calibration found - starting auto-calibration");
  }

  return true;
}

/**
 * Check if calibration data exists in NVS
 */
bool BNO055Manager::hasCalibrationInNVS() {
  prefs.begin(NVS_NAMESPACE, true); // Read-only
  bool hasData = prefs.isKey("accel_x");
  prefs.end();
  return hasData;
}

/**
 * Load calibration offsets from NVS
 */
void BNO055Manager::loadCalibrationFromNVS() {
  prefs.begin(NVS_NAMESPACE, true); // Read-only

  adafruit_bno055_offsets_t offsets;

  offsets.accel_offset_x = prefs.getShort("accel_x", 0);
  offsets.accel_offset_y = prefs.getShort("accel_y", 0);
  offsets.accel_offset_z = prefs.getShort("accel_z", 0);

  offsets.mag_offset_x = prefs.getShort("mag_x", 0);
  offsets.mag_offset_y = prefs.getShort("mag_y", 0);
  offsets.mag_offset_z = prefs.getShort("mag_z", 0);

  offsets.gyro_offset_x = prefs.getShort("gyro_x", 0);
  offsets.gyro_offset_y = prefs.getShort("gyro_y", 0);
  offsets.gyro_offset_z = prefs.getShort("gyro_z", 0);

  offsets.accel_radius = prefs.getShort("accel_r", 0);
  offsets.mag_radius = prefs.getShort("mag_r", 0);

  prefs.end();

  // Switch to CONFIG mode to write offsets
  bno.setMode(OPERATION_MODE_CONFIG);
  delay(25);

  // Write offsets
  bno.setSensorOffsets(offsets);
  delay(10);

  // Switch back to NDOF mode
  bno.setMode(OPERATION_MODE_NDOF);
  delay(20);

  Serial.println("Calibration offsets applied");
}

/**
 * Save calibration offsets to NVS
 */
void BNO055Manager::saveCalibrationToNVS() {
  adafruit_bno055_offsets_t offsets;

  if (!bno.getSensorOffsets(offsets)) {
    Serial.println("ERROR: Failed to read sensor offsets");
    return;
  }

  prefs.begin(NVS_NAMESPACE, false); // Read-write

  prefs.putShort("accel_x", offsets.accel_offset_x);
  prefs.putShort("accel_y", offsets.accel_offset_y);
  prefs.putShort("accel_z", offsets.accel_offset_z);

  prefs.putShort("mag_x", offsets.mag_offset_x);
  prefs.putShort("mag_y", offsets.mag_offset_y);
  prefs.putShort("mag_z", offsets.mag_offset_z);

  prefs.putShort("gyro_x", offsets.gyro_offset_x);
  prefs.putShort("gyro_y", offsets.gyro_offset_y);
  prefs.putShort("gyro_z", offsets.gyro_offset_z);

  prefs.putShort("accel_r", offsets.accel_radius);
  prefs.putShort("mag_r", offsets.mag_radius);

  prefs.end();

  Serial.println("Calibration saved to NVS");
}

/**
 * Update sensor and calibration state
 */
void BNO055Manager::update() {
  // Read sensor data
  sensors_event_t event;
  bno.getEvent(&event);

  // Update heading filter
  float rawHeading = (float)event.orientation.x;
  if (rawHeading >= 0 && rawHeading < 360) {
    updateFilteredHeading(rawHeading);
  }

  // Update calibration state every 500ms
  unsigned long now = millis();
  if (now - lastCalibCheckTime > 500) {
    lastCalibCheckTime = now;
    updateCalibrationState();
  }

  // Update warning state
  if (state == STATE_NORMAL) {
    updateWarningState();
  }
}

/**
 * Update calibration state machine
 */
void BNO055Manager::updateCalibrationState() {
  uint8_t sys, gyro, accel, mag;
  bno.getCalibration(&sys, &gyro, &accel, &mag);

  lastSys = sys;
  lastGyro = gyro;
  lastAccel = accel;
  lastMag = mag;

  bool fullyCalibrated = (sys == 3 && gyro == 3 && accel == 3 && mag == 3);

  switch (state) {
    case STATE_AUTO_CALIBRATING:
      if (fullyCalibrated) {
        if (!wasFullyCalibrated) {
          // Just became fully calibrated
          wasFullyCalibrated = true;
          fullyCalibStartTime = millis();
          Serial.println("Fully calibrated - waiting for stability...");
        } else {
          // Check if stable for long enough
          if (millis() - fullyCalibStartTime >= CALIB_STABLE_TIME) {
            // Save and transition to normal
            saveCalibrationToNVS();
            state = STATE_NORMAL;
            calibrationLoaded = true;
            Serial.println("Auto-calibration complete!");
          }
        }
      } else {
        // Lost full calibration
        wasFullyCalibrated = false;
      }
      break;

    case STATE_MANUAL_CALIBRATING:
      if (fullyCalibrated) {
        if (!wasFullyCalibrated) {
          wasFullyCalibrated = true;
          fullyCalibStartTime = millis();
          Serial.println("Fully calibrated - waiting for stability...");
        } else {
          if (millis() - fullyCalibStartTime >= CALIB_STABLE_TIME) {
            saveCalibrationToNVS();
            state = STATE_NORMAL;
            Serial.println("Manual calibration complete!");
          }
        }
      } else {
        wasFullyCalibrated = false;
      }
      break;

    case STATE_NORMAL:
      // Nothing special to do
      break;
  }
}

/**
 * Update warning state (only in NORMAL mode)
 */
void BNO055Manager::updateWarningState() {
  unsigned long now = millis();

  if (now - lastWarningCheckTime < WARNING_CHECK_INTERVAL) {
    return;
  }

  lastWarningCheckTime = now;

  // Check if calibration is poor
  bool poorCalib = (lastSys < WARNING_SYS_THRESHOLD || lastMag < WARNING_MAG_THRESHOLD);

  if (poorCalib) {
    if (!warningActive) {
      warningActive = true;
      Serial.println("WARNING: Poor calibration detected!");
    }
  } else {
    if (warningActive) {
      warningActive = false;
      Serial.println("Calibration OK");
    }
  }
}

/**
 * Calculate angle difference with wrap-around
 */
float BNO055Manager::angleDiff(float a, float b) {
  float d = a - b;
  while (d > 180.0f) d -= 360.0f;
  while (d < -180.0f) d += 360.0f;
  return d;
}

/**
 * Update filtered heading with exponential smoothing
 */
void BNO055Manager::updateFilteredHeading(float rawHeading) {
  if (!headingInitialized) {
    filteredHeading = rawHeading;
    headingInitialized = true;
    return;
  }

  float diff = angleDiff(rawHeading, filteredHeading);
  filteredHeading += HEADING_FILTER_ALPHA * diff;

  // Normalize to 0-360
  if (filteredHeading < 0) filteredHeading += 360.0f;
  if (filteredHeading >= 360.0f) filteredHeading -= 360.0f;
}

/**
 * Get filtered heading
 */
float BNO055Manager::getFilteredHeadingDegrees() {
  return filteredHeading;
}

/**
 * Get calibration status
 */
void BNO055Manager::getCalibrationStatus(uint8_t& sys, uint8_t& gyro, uint8_t& accel, uint8_t& mag) {
  sys = lastSys;
  gyro = lastGyro;
  accel = lastAccel;
  mag = lastMag;
}

/**
 * Check if fully calibrated
 */
bool BNO055Manager::isFullyCalibrated() {
  return (lastSys == 3 && lastGyro == 3 && lastAccel == 3 && lastMag == 3);
}

/**
 * Check if calibration was loaded
 */
bool BNO055Manager::hasValidCalibrationLoaded() {
  return calibrationLoaded;
}

/**
 * Check if warning is active
 */
bool BNO055Manager::isWarningActive() {
  return warningActive;
}

/**
 * Request manual calibration
 */
void BNO055Manager::requestManualCalibration() {
  if (state != STATE_MANUAL_CALIBRATING) {
    state = STATE_MANUAL_CALIBRATING;
    wasFullyCalibrated = false;
    Serial.println("Manual calibration started");
  }
}

/**
 * Exit manual calibration (also cancels auto-calibration)
 */
void BNO055Manager::exitManualCalibration() {
  if (state == STATE_MANUAL_CALIBRATING || state == STATE_AUTO_CALIBRATING) {
    CalibrationState oldState = state;
    state = STATE_NORMAL;
    if (oldState == STATE_AUTO_CALIBRATING) {
      Serial.println("Auto-calibration cancelled - continuing without calibration");
    } else {
      Serial.println("Manual calibration cancelled");
    }
  }
}

/**
 * Get current calibration state
 */
CalibrationState BNO055Manager::getCalibrationState() {
  return state;
}

/**
 * Get direction text
 */
const char* BNO055Manager::getDirectionText() {
  float h = filteredHeading;

  if (h >= 337.5f || h < 22.5f) return "N";
  if (h >= 22.5f && h < 67.5f) return "NE";
  if (h >= 67.5f && h < 112.5f) return "E";
  if (h >= 112.5f && h < 157.5f) return "SE";
  if (h >= 157.5f && h < 202.5f) return "S";
  if (h >= 202.5f && h < 247.5f) return "SW";
  if (h >= 247.5f && h < 292.5f) return "W";
  if (h >= 292.5f && h < 337.5f) return "NW";

  return "?";
}

/**
 * Attach UI for callbacks
 */
void BNO055Manager::attachUI(CompassUI* uiPtr) {
  ui = uiPtr;
}
