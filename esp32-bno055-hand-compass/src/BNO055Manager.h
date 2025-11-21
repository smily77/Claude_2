/**
 * BNO055Manager.h
 *
 * Manages the BNO055 9-axis IMU sensor for compass functionality
 * - Handles sensor initialization and I2C communication
 * - Manages calibration state (auto/manual)
 * - Stores/loads calibration data from NVS (Preferences)
 * - Provides filtered heading data with wrap-around handling
 * - Implements warning logic for poor calibration
 */

#ifndef BNO055_MANAGER_H
#define BNO055_MANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>
#include <Preferences.h>

// Forward declaration
class CompassUI;

/**
 * Calibration state machine
 */
enum CalibrationState {
  STATE_NORMAL,              // Normal operation
  STATE_AUTO_CALIBRATING,    // Auto-calibration in progress
  STATE_MANUAL_CALIBRATING   // Manual calibration requested
};

/**
 * BNO055Manager class
 */
class BNO055Manager {
public:
  BNO055Manager();
  ~BNO055Manager();

  /**
   * Initialize the BNO055 sensor
   * - Sets up I2C communication
   * - Initializes the sensor
   * - Loads calibration from NVS if available
   * - Starts auto-calibration if no data found
   * @return true if successful, false on error
   */
  bool begin();

  /**
   * Update sensor readings and calibration state
   * Call this regularly in loop()
   */
  void update();

  /**
   * Get the filtered heading in degrees (0-359.9)
   * @return heading in degrees, North = 0, East = 90
   */
  float getFilteredHeadingDegrees();

  /**
   * Get calibration status for all subsystems
   * @param sys System calibration (0-3)
   * @param gyro Gyroscope calibration (0-3)
   * @param accel Accelerometer calibration (0-3)
   * @param mag Magnetometer calibration (0-3)
   */
  void getCalibrationStatus(uint8_t& sys, uint8_t& gyro, uint8_t& accel, uint8_t& mag);

  /**
   * Check if sensor is fully calibrated (all values == 3)
   * @return true if fully calibrated
   */
  bool isFullyCalibrated();

  /**
   * Check if valid calibration data was loaded from NVS
   * @return true if calibration was loaded successfully
   */
  bool hasValidCalibrationLoaded();

  /**
   * Check if warning should be shown
   * (poor calibration in normal mode for extended time)
   * @return true if warning active
   */
  bool isWarningActive();

  /**
   * Request manual calibration mode
   */
  void requestManualCalibration();

  /**
   * Exit manual calibration mode
   */
  void exitManualCalibration();

  /**
   * Get current calibration state
   * @return current CalibrationState
   */
  CalibrationState getCalibrationState();

  /**
   * Get direction text (N, NE, E, SE, S, SW, W, NW)
   * @return pointer to direction string
   */
  const char* getDirectionText();

  /**
   * Attach UI for callbacks (optional)
   * @param ui pointer to CompassUI instance
   */
  void attachUI(CompassUI* ui);

private:
  Adafruit_BNO055 bno;
  Preferences prefs;
  CompassUI* ui;

  // I2C pins
  int sdaPin;
  int sclPin;

  // Calibration state
  CalibrationState state;
  bool calibrationLoaded;
  uint8_t lastSys, lastGyro, lastAccel, lastMag;

  // Timing
  unsigned long lastCalibCheckTime;
  unsigned long lastWarningCheckTime;
  unsigned long fullyCalibStartTime;
  bool wasFullyCalibrated;
  bool warningActive;

  // Heading filter
  float filteredHeading;
  bool headingInitialized;

  // Private methods
  void loadCalibrationFromNVS();
  void saveCalibrationToNVS();
  bool hasCalibrationInNVS();
  void updateCalibrationState();
  void updateWarningState();
  float angleDiff(float a, float b);
  void updateFilteredHeading(float rawHeading);
  void determineI2CPins();
};

#endif // BNO055_MANAGER_H
