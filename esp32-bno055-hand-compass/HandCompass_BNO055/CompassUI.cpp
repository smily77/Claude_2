/**
 * CompassUI.cpp
 *
 * Implementation of the compass user interface
 */

#include "CompassUI.h"
#include "BNO055Manager.h"
#include <math.h>

// Compass dimensions
#define COMPASS_RADIUS 100
#define NEEDLE_LENGTH 80
#define CARDINAL_OFFSET 15

// Button dimensions
#define BUTTON_X 10
#define BUTTON_Y 10
#define BUTTON_W 100
#define BUTTON_H 40

/**
 * Constructor
 */
CompassUI::CompassUI()
  : display(nullptr),
    manager(nullptr),
    screenWidth(0),
    screenHeight(0),
    centerX(0),
    centerY(0),
    currentHeading(0.0f),
    currentDirection("N"),
    lastDrawnHeading(-999.0f),
    calSys(0), calGyro(0), calAccel(0), calMag(0),
    lastCalSys(255), lastCalGyro(255), lastCalAccel(255), lastCalMag(255),
    showWarning(false),
    lastShowWarning(false),
    inCalibrationMode(false),
    lastInCalibrationMode(false),
    autoCalibrationMode(false),
    needsFullRedraw(true),
    lastTouchState(false),
    buttonPressed(false),
    lastButtonPressed(false) {
}

/**
 * Initialize UI
 */
void CompassUI::begin(LGFX* lcd) {
  display = lcd;

  screenWidth = display->width();
  screenHeight = display->height();
  centerX = screenWidth / 2;
  centerY = screenHeight / 2;

  display->fillScreen(COLOR_BACKGROUND);

  Serial.printf("UI initialized: %dx%d\n", screenWidth, screenHeight);
}

/**
 * Main UI loop
 */
void CompassUI::loop() {
  handleTouch();

  // Check what changed
  bool headingChanged = abs(currentHeading - lastDrawnHeading) > 0.5f;
  bool calChanged = (calSys != lastCalSys || calGyro != lastCalGyro ||
                     calAccel != lastCalAccel || calMag != lastCalMag);
  bool warningChanged = (showWarning != lastShowWarning);
  bool modeChanged = (inCalibrationMode != lastInCalibrationMode);
  bool buttonChanged = (buttonPressed != lastButtonPressed);

  // Full redraw only when necessary (first run or mode change)
  if (needsFullRedraw || modeChanged) {
    display->fillScreen(COLOR_BACKGROUND);
    drawCompassRose();
    drawNeedle(currentHeading);
    drawDigitalDisplay();
    drawCalibrationPanel();
    drawCalibrateButton();

    if (inCalibrationMode) {
      drawCalibrationInstructions();
    }

    needsFullRedraw = false;
    lastDrawnHeading = currentHeading;
    lastCalSys = calSys; lastCalGyro = calGyro;
    lastCalAccel = calAccel; lastCalMag = calMag;
    lastShowWarning = showWarning;
    lastInCalibrationMode = inCalibrationMode;
    lastButtonPressed = buttonPressed;
    return;
  }

  // Partial updates only when needed - no flicker!
  if (headingChanged) {
    // Clear and redraw compass area only
    display->fillCircle(centerX, centerY, COMPASS_RADIUS + 5, COLOR_BACKGROUND);
    drawCompassRose();
    drawNeedle(currentHeading);
    // Also update digital display (heading number)
    display->fillRect(centerX - 80, 15, 160, 50, COLOR_BACKGROUND);
    drawDigitalDisplay();
    lastDrawnHeading = currentHeading;
  }

  if (calChanged || warningChanged) {
    // Clear and redraw calibration panel only
    display->fillRect(0, screenHeight - 65, screenWidth, 65, COLOR_BACKGROUND);
    drawCalibrationPanel();
    lastCalSys = calSys; lastCalGyro = calGyro;
    lastCalAccel = calAccel; lastCalMag = calMag;
    lastShowWarning = showWarning;
  }

  if (buttonChanged) {
    // Redraw button only
    drawCalibrateButton();
    lastButtonPressed = buttonPressed;
  }
}

/**
 * Set heading and direction
 */
void CompassUI::setHeading(float degrees, const char* dirText) {
  currentHeading = degrees;
  currentDirection = dirText;
}

/**
 * Set calibration status
 */
void CompassUI::setCalibrationStatus(uint8_t sys, uint8_t gyro, uint8_t accel, uint8_t mag, bool warning) {
  calSys = sys;
  calGyro = gyro;
  calAccel = accel;
  calMag = mag;
  showWarning = warning;
}

/**
 * Set calibration mode
 */
void CompassUI::setCalibrationMode(bool inCalibration, bool autoMode) {
  inCalibrationMode = inCalibration;
  autoCalibrationMode = autoMode;
}

/**
 * Show error message (blocking)
 */
void CompassUI::showError(const char* message) {
  display->fillScreen(COLOR_BACKGROUND);
  display->setTextColor(COLOR_WARNING);
  display->setTextDatum(middle_center);
  display->setFont(&fonts::FreeSansBold12pt7b);
  display->drawString("ERROR", centerX, centerY - 30);
  display->setFont(&fonts::FreeSans9pt7b);
  display->drawString(message, centerX, centerY + 10);
}

/**
 * Attach manager for callbacks
 */
void CompassUI::attachManager(BNO055Manager* mgr) {
  manager = mgr;
}

/**
 * Draw compass rose (fixed with rotating needle)
 */
void CompassUI::drawCompassRose() {
  // Draw outer circle
  display->drawCircle(centerX, centerY, COMPASS_RADIUS, COLOR_COMPASS_RING);
  display->drawCircle(centerX, centerY, COMPASS_RADIUS - 1, COLOR_COMPASS_RING);

  // Draw cardinal directions
  drawCardinalMark(0, "N", true);    // North (red)
  drawCardinalMark(90, "E", false);   // East
  drawCardinalMark(180, "S", false);  // South
  drawCardinalMark(270, "W", false);  // West

  // Draw intermediate directions (smaller)
  display->setFont(&fonts::FreeSans9pt7b);
  drawCardinalMark(45, "NE", false);
  drawCardinalMark(135, "SE", false);
  drawCardinalMark(225, "SW", false);
  drawCardinalMark(315, "NW", false);

  // Draw center dot
  display->fillCircle(centerX, centerY, 3, COLOR_COMPASS_RING);
}

/**
 * Draw cardinal mark on compass
 */
void CompassUI::drawCardinalMark(float angle, const char* label, bool isNorth) {
  float rad = angle * PI / 180.0f;
  int16_t x = centerX + (COMPASS_RADIUS + CARDINAL_OFFSET) * sin(rad);
  int16_t y = centerY - (COMPASS_RADIUS + CARDINAL_OFFSET) * cos(rad);

  uint16_t color = isNorth ? COLOR_NORTH : COLOR_CARDINAL;

  display->setTextColor(color);
  display->setTextDatum(middle_center);

  if (isNorth) {
    display->setFont(&fonts::FreeSansBold12pt7b);
  } else {
    display->setFont(&fonts::FreeSans9pt7b);
  }

  display->drawString(label, x, y);

  // Draw tick mark
  int16_t x1 = centerX + (COMPASS_RADIUS - 10) * sin(rad);
  int16_t y1 = centerY - (COMPASS_RADIUS - 10) * cos(rad);
  int16_t x2 = centerX + COMPASS_RADIUS * sin(rad);
  int16_t y2 = centerY - COMPASS_RADIUS * cos(rad);
  display->drawLine(x1, y1, x2, y2, color);
}

/**
 * Draw compass needle
 */
void CompassUI::drawNeedle(float heading) {
  float rad = heading * PI / 180.0f;

  // Needle point (pointing to heading)
  int16_t x_point = centerX + NEEDLE_LENGTH * sin(rad);
  int16_t y_point = centerY - NEEDLE_LENGTH * cos(rad);

  // Needle tail (opposite direction)
  int16_t x_tail = centerX - 20 * sin(rad);
  int16_t y_tail = centerY + 20 * cos(rad);

  // Draw needle
  display->drawLine(centerX, centerY, x_point, y_point, COLOR_NEEDLE);
  display->drawLine(centerX, centerY, x_tail, y_tail, COLOR_NEEDLE);

  // Draw arrow head
  float arrowAngle = 20.0f * PI / 180.0f;
  float arrowLen = 15.0f;

  int16_t arrow1_x = x_point - arrowLen * sin(rad - arrowAngle);
  int16_t arrow1_y = y_point + arrowLen * cos(rad - arrowAngle);
  int16_t arrow2_x = x_point - arrowLen * sin(rad + arrowAngle);
  int16_t arrow2_y = y_point + arrowLen * cos(rad + arrowAngle);

  display->drawLine(x_point, y_point, arrow1_x, arrow1_y, COLOR_NEEDLE);
  display->drawLine(x_point, y_point, arrow2_x, arrow2_y, COLOR_NEEDLE);
}

/**
 * Draw digital heading display
 */
void CompassUI::drawDigitalDisplay() {
  // Position at top of screen
  int16_t y = 20;

  display->setTextColor(COLOR_TEXT);
  display->setTextDatum(top_center);

  // Large heading number
  display->setFont(&fonts::FreeSansBold18pt7b);
  char headingStr[16];
  sprintf(headingStr, "%.1f°", currentHeading);
  display->drawString(headingStr, centerX, y);

  // Direction text below
  display->setFont(&fonts::FreeSansBold12pt7b);
  display->drawString(currentDirection, centerX, y + 35);
}

/**
 * Draw calibration status panel
 */
void CompassUI::drawCalibrationPanel() {
  // Position at bottom of screen
  int16_t y = screenHeight - 60;
  int16_t x = 10;

  display->setFont(&fonts::FreeSans9pt7b);
  display->setTextDatum(top_left);

  // Draw status bars
  char buf[32];

  sprintf(buf, "SYS:%d", calSys);
  display->setTextColor(getCalibrationColor(calSys));
  display->drawString(buf, x, y);

  sprintf(buf, "GYR:%d", calGyro);
  display->setTextColor(getCalibrationColor(calGyro));
  display->drawString(buf, x + 60, y);

  sprintf(buf, "ACC:%d", calAccel);
  display->setTextColor(getCalibrationColor(calAccel));
  display->drawString(buf, x, y + 20);

  sprintf(buf, "MAG:%d", calMag);
  display->setTextColor(getCalibrationColor(calMag));
  display->drawString(buf, x + 60, y + 20);

  // Warning indicator
  if (showWarning) {
    display->setTextColor(COLOR_WARNING);
    display->setFont(&fonts::FreeSansBold9pt7b);
    display->setTextDatum(top_right);
    display->drawString("! CALIBRATE", screenWidth - 10, y + 10);
  }
}

/**
 * Draw calibration instructions
 */
void CompassUI::drawCalibrationInstructions() {
  int16_t y = screenHeight - 120;

  display->setFont(&fonts::FreeSans9pt7b);
  display->setTextColor(COLOR_WARNING);
  display->setTextDatum(top_center);

  if (autoCalibrationMode) {
    display->drawString("AUTO-CALIBRATION", centerX, y);
  } else {
    display->drawString("MANUAL CALIBRATION", centerX, y);
  }

  display->setTextColor(COLOR_TEXT);
  display->setFont(&fonts::FreeSans9pt7b);
  y += 20;

  display->drawString("Move device in", centerX, y);
  y += 15;
  display->drawString("figure-8 pattern", centerX, y);
  y += 15;
  display->drawString("for all axes", centerX, y);
}

/**
 * Draw calibrate button
 */
void CompassUI::drawCalibrateButton() {
  uint16_t btnColor = buttonPressed ? COLOR_BUTTON_PRESS : COLOR_BUTTON;

  display->fillRoundRect(BUTTON_X, BUTTON_Y, BUTTON_W, BUTTON_H, 5, btnColor);
  display->drawRoundRect(BUTTON_X, BUTTON_Y, BUTTON_W, BUTTON_H, 5, COLOR_TEXT);

  display->setFont(&fonts::FreeSans9pt7b);
  display->setTextColor(COLOR_TEXT);
  display->setTextDatum(middle_center);

  const char* btnText = inCalibrationMode ? "CANCEL" : "CALIBRATE";
  display->drawString(btnText, BUTTON_X + BUTTON_W / 2, BUTTON_Y + BUTTON_H / 2);
}

/**
 * Handle touch input
 */
void CompassUI::handleTouch() {
  if (!display) return;

  uint16_t x, y;
  bool touching = display->getTouch(&x, &y);

  // Check if currently touching button area
  bool touchingButton = false;
  if (touching) {
    touchingButton = (x >= BUTTON_X && x <= BUTTON_X + BUTTON_W &&
                      y >= BUTTON_Y && y <= BUTTON_Y + BUTTON_H);
  }

  // Detect button press on release (standard button behavior)
  if (lastTouchState && !touching) {
    // Touch was released
    if (buttonPressed) {
      // Button was pressed and now released - trigger action!
      Serial.println("Button clicked!");
      if (manager) {
        if (inCalibrationMode) {
          Serial.println("Exiting calibration...");
          manager->exitManualCalibration();
        } else {
          Serial.println("Starting calibration...");
          manager->requestManualCalibration();
        }
      }
    }
    buttonPressed = false;
  } else if (touching) {
    // Update button state based on current touch position
    buttonPressed = touchingButton;
  } else {
    // Not touching
    buttonPressed = false;
  }

  lastTouchState = touching;
}

/**
 * Get color based on calibration value
 */
uint16_t CompassUI::getCalibrationColor(uint8_t value) {
  if (value == 3) return COLOR_CALIB_OK;
  if (value >= 1) return COLOR_CALIB_POOR;
  return COLOR_WARNING;
}
