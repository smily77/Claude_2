/**
 * HandCompass_BNO055.ino
 *
 * ESP32 Hand Compass with BNO055 9-axis IMU sensor
 *
 * Features:
 * - Beautiful compass rose display with rotating needle
 * - Digital heading display with cardinal direction
 * - Automatic calibration with NVS storage
 * - Manual calibration mode with touch button
 * - Real-time calibration status display
 * - Smooth heading filtering
 * - Warning indicators for poor calibration
 *
 * Hardware:
 * - ESP32 or ESP32-S3
 * - BNO055 IMU sensor (I2C)
 * - TFT Display 320x280 with LovyanGFX
 *
 * Libraries required:
 * - LovyanGFX
 * - Adafruit BNO055
 * - Adafruit Unified Sensor
 *
 * Author: Claude AI
 * Date: 2025
 */

#include "CYD_Display_Config.h"
#include "CompassUI.h"
#include "BNO055Manager.h"

// Global instances
static LGFX lcd;
static CompassUI ui;
static BNO055Manager compass;

/**
 * Setup function - runs once at startup
 */
void setup() {
  // Initialize serial for debugging
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n=================================");
  Serial.println("ESP32 Hand Compass - BNO055");
  Serial.println("=================================\n");

  // Initialize display
  Serial.println("Initializing display...");
  lcd.init();
  lcd.setBrightness(200);
  lcd.setColorDepth(16);
  lcd.fillScreen(TFT_BLACK);

  // Initialize UI
  Serial.println("Initializing UI...");
  ui.begin(&lcd);

  // Initialize compass sensor
  Serial.println("Initializing BNO055 sensor...");
  if (!compass.begin()) {
    ui.showError("BNO055 not found!");
    Serial.println("FATAL: BNO055 initialization failed!");
    while (true) {
      delay(1000);
    }
  }

  // Connect UI and compass manager
  compass.attachUI(&ui);
  ui.attachManager(&compass);

  Serial.println("\n=================================");
  Serial.println("System ready!");
  Serial.println("=================================\n");

  delay(500);
}

/**
 * Main loop - runs continuously
 */
void loop() {
  // Update compass sensor readings and calibration state
  compass.update();

  // Get current heading and direction
  float heading = compass.getFilteredHeadingDegrees();
  const char* direction = compass.getDirectionText();

  // Get calibration status
  uint8_t sys, gyro, accel, mag;
  compass.getCalibrationStatus(sys, gyro, accel, mag);
  bool warning = compass.isWarningActive();

  // Get calibration mode
  CalibrationState calState = compass.getCalibrationState();
  bool inCalibration = (calState != STATE_NORMAL);
  bool autoCalib = (calState == STATE_AUTO_CALIBRATING);

  // Update UI
  ui.setHeading(heading, direction);
  ui.setCalibrationStatus(sys, gyro, accel, mag, warning);
  ui.setCalibrationMode(inCalibration, autoCalib);

  // Render UI
  ui.loop();

  // Small delay to prevent excessive CPU usage
  delay(50);
}
