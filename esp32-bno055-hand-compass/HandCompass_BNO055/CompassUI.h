/**
 * CompassUI.h
 *
 * User interface for the hand compass
 * - Renders compass rose (rotating or fixed)
 * - Shows digital heading and direction
 * - Displays calibration status panel
 * - Handles touch input for calibration button
 * - Shows calibration instructions when needed
 * - Warning indicators
 */

#ifndef COMPASS_UI_H
#define COMPASS_UI_H

#include <Arduino.h>
#include "CYD_Display_Config.h"

// Forward declaration
class BNO055Manager;

/**
 * UI Themes and Colors
 */
#define COLOR_BACKGROUND   0x0000  // Black
#define COLOR_COMPASS_RING 0xFFFF  // White
#define COLOR_NORTH        0xF800  // Red
#define COLOR_CARDINAL     0xFFFF  // White
#define COLOR_NEEDLE       0xF800  // Red
#define COLOR_TEXT         0xFFFF  // White
#define COLOR_WARNING      0xF800  // Red
#define COLOR_CALIB_OK     0x07E0  // Green
#define COLOR_CALIB_POOR   0xFFE0  // Yellow
#define COLOR_BUTTON       0x4A49  // Gray
#define COLOR_BUTTON_PRESS 0x2104  // Dark Gray

/**
 * CompassUI class
 */
class CompassUI {
public:
  CompassUI();

  /**
   * Initialize the UI
   * @param lcd pointer to LGFX display instance
   */
  void begin(LGFX* lcd);

  /**
   * Update loop - handle touch and animations
   * Call this regularly in loop()
   */
  void loop();

  /**
   * Set current heading and direction
   * @param degrees heading in degrees (0-359.9)
   * @param dirText direction text (N, NE, E, etc.)
   */
  void setHeading(float degrees, const char* dirText);

  /**
   * Set calibration status
   * @param sys system calibration (0-3)
   * @param gyro gyroscope calibration (0-3)
   * @param accel accelerometer calibration (0-3)
   * @param mag magnetometer calibration (0-3)
   * @param warning true if warning should be shown
   */
  void setCalibrationStatus(uint8_t sys, uint8_t gyro, uint8_t accel, uint8_t mag, bool warning);

  /**
   * Set calibration mode state
   * @param inCalibration true if calibrating
   * @param autoMode true if auto-calibration, false if manual
   */
  void setCalibrationMode(bool inCalibration, bool autoMode);

  /**
   * Show error message (blocking)
   * @param message error message to display
   */
  void showError(const char* message);

  /**
   * Attach BNO055 manager for callbacks
   * @param manager pointer to BNO055Manager instance
   */
  void attachManager(BNO055Manager* manager);

private:
  LGFX* display;
  BNO055Manager* manager;

  // Display dimensions
  int16_t screenWidth;
  int16_t screenHeight;
  int16_t centerX;
  int16_t centerY;

  // Current state
  float currentHeading;
  const char* currentDirection;
  uint8_t calSys, calGyro, calAccel, calMag;
  bool showWarning;
  bool inCalibrationMode;
  bool autoCalibrationMode;

  // Touch state
  bool lastTouchState;
  bool buttonPressed;

  // Rendering
  void drawCompassRose();
  void drawNeedle(float heading);
  void drawDigitalDisplay();
  void drawCalibrationPanel();
  void drawCalibrationInstructions();
  void drawCalibrateButton();
  void handleTouch();

  // Helper functions
  uint16_t getCalibrationColor(uint8_t value);
  void drawCardinalMark(float angle, const char* label, bool isNorth);
};

#endif // COMPASS_UI_H
