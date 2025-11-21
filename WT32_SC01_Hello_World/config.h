/*
 * WT32-SC01 Konfiguration für LovyanGFX
 *
 * Diese Datei enthält alle Display-spezifischen Einstellungen.
 * Keine Änderungen in der Bibliothek notwendig!
 *
 * Hardware: WT32-SC01
 * - Display: 3.5" TFT LCD (480x320 Pixel)
 * - Controller: ST7796
 * - Touch: FT6336U (Capacitive Touch)
 * - MCU: ESP32-WROVER
 */

#ifndef CONFIG_H
#define CONFIG_H

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

// WT32-SC01 Konfiguration
class LGFX : public lgfx::LGFX_Device
{
  lgfx::Panel_ST7796 _panel_instance;
  lgfx::Bus_SPI _bus_instance;
  lgfx::Light_PWM _light_instance;
  lgfx::Touch_FT5x06 _touch_instance;  // FT6336U ist kompatibel mit FT5x06

public:
  LGFX(void)
  {
    {
      // SPI Bus Konfiguration
      auto cfg = _bus_instance.config();

      cfg.spi_host = HSPI_HOST;  // WT32-SC01 verwendet HSPI
      cfg.spi_mode = 0;
      cfg.freq_write = 40000000; // 40MHz
      cfg.freq_read = 16000000;  // 16MHz
      cfg.spi_3wire = true;
      cfg.use_lock = true;
      cfg.dma_channel = 1;

      // WT32-SC01 SPI Pins
      cfg.pin_sclk = 14;
      cfg.pin_mosi = 13;
      cfg.pin_miso = 12;
      cfg.pin_dc = 21;

      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }

    {
      // Backlight Konfiguration
      auto cfg = _light_instance.config();

      cfg.pin_bl = 23;
      cfg.invert = false;
      cfg.freq = 44100;
      cfg.pwm_channel = 7;

      _light_instance.config(cfg);
      _panel_instance.setLight(&_light_instance);
    }

    {
      // Display Panel Konfiguration
      auto cfg = _panel_instance.config();

      cfg.pin_cs = 15;
      cfg.pin_rst = -1;  // Kein Reset Pin
      cfg.pin_busy = -1;

      cfg.panel_width = 320;
      cfg.panel_height = 480;

      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.offset_rotation = 0;

      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits = 1;
      cfg.readable = true;
      cfg.invert = true;
      cfg.rgb_order = false;
      cfg.dlen_16bit = false;
      cfg.bus_shared = true;

      _panel_instance.config(cfg);
    }

    {
      // Touch Panel Konfiguration
      auto cfg = _touch_instance.config();

      cfg.x_min = 0;
      cfg.x_max = 319;
      cfg.y_min = 0;
      cfg.y_max = 479;
      cfg.pin_int = -1;   // Interrupt Pin (optional)
      cfg.pin_rst = -1;   // Reset Pin (optional)
      cfg.bus_shared = true;
      cfg.offset_rotation = 0;

      // I2C Konfiguration für Touch
      cfg.i2c_port = 1;   // I2C Port
      cfg.i2c_addr = 0x38; // FT6336U I2C Adresse
      cfg.pin_sda = 18;   // I2C SDA
      cfg.pin_scl = 19;   // I2C SCL
      cfg.freq = 400000;  // 400kHz

      _touch_instance.config(cfg);
      _panel_instance.setTouch(&_touch_instance);
    }

    setPanel(&_panel_instance);
  }
};

#endif // CONFIG_H
