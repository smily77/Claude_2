/**
 * CYD_Display_Config.h
 *
 * This is a placeholder header file.
 *
 * IMPORTANT: This file should be provided by the user and contain
 * the LovyanGFX configuration for their specific display hardware.
 *
 * Requirements:
 * - Must define a class named 'LGFX' that extends from LGFX_Device
 * - Must include all necessary LovyanGFX headers
 * - Must configure the display panel, bus, and touch interface
 * - Display resolution should be 320x280 (or similar TFT)
 *
 * Example structure:
 *
 * #define LGFX_USE_V1
 * #include <LovyanGFX.hpp>
 *
 * class LGFX : public lgfx::LGFX_Device {
 *   lgfx::Panel_ILI9341 _panel_instance;
 *   lgfx::Bus_SPI _bus_instance;
 *   lgfx::Touch_XPT2046 _touch_instance;
 *
 * public:
 *   LGFX(void) {
 *     // Configure panel, bus, touch...
 *   }
 * };
 *
 * For a working implementation, please refer to your CYD (Cheap Yellow Display)
 * or other ESP32 TFT display configuration examples.
 */

#ifndef CYD_DISPLAY_CONFIG_H
#define CYD_DISPLAY_CONFIG_H

#warning "CYD_Display_Config.h: Using placeholder configuration!"
#warning "Please replace this file with your actual LovyanGFX display configuration."

// Placeholder includes - replace with your actual configuration
#define LGFX_USE_V1
#include <LovyanGFX.hpp>

// Placeholder LGFX class - replace with your actual display configuration
class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ILI9341 _panel_instance;
  lgfx::Bus_SPI _bus_instance;
  lgfx::Touch_XPT2046 _touch_instance;

public:
  LGFX(void) {
    // TODO: Add your display configuration here
    // This is just a placeholder!
  }
};

#endif // CYD_DISPLAY_CONFIG_H
