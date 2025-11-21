/*
 * WT32-SC01 Alternative Konfiguration für LovyanGFX
 *
 * Falls die Standard-Config nicht funktioniert, probiere diese aus.
 * Um diese zu verwenden: Benenne config.h um und benenne diese Datei zu config.h
 */

#ifndef CONFIG_H
#define CONFIG_H

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

// WT32-SC01 Konfiguration (Alternative Einstellungen)
class LGFX : public lgfx::LGFX_Device
{
  lgfx::Panel_ST7796 _panel_instance;
  lgfx::Bus_SPI _bus_instance;
  lgfx::Light_PWM _light_instance;
  lgfx::Touch_FT5x06 _touch_instance;

public:
  LGFX(void)
  {
    {
      // SPI Bus Konfiguration
      auto cfg = _bus_instance.config();

      cfg.spi_host = VSPI_HOST;
      cfg.spi_mode = 0;
      cfg.freq_write = 20000000; // Langsamer: 20MHz statt 40MHz
      cfg.freq_read = 16000000;
      cfg.spi_3wire = true;
      cfg.use_lock = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;

      // WT32-SC01 SPI Pins (verifiziert)
      cfg.pin_sclk = 14;
      cfg.pin_mosi = 13;
      cfg.pin_miso = -1;
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
      cfg.pin_rst = 22;
      cfg.pin_busy = -1;

      cfg.panel_width = 320;
      cfg.panel_height = 480;

      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.offset_rotation = 0;

      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits = 1;
      cfg.readable = false;        // GEÄNDERT: false statt true
      cfg.invert = false;          // GEÄNDERT: false statt true (WICHTIG!)
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
      cfg.pin_int = -1;
      cfg.pin_rst = -1;
      cfg.bus_shared = true;
      cfg.offset_rotation = 0;

      // I2C Konfiguration für Touch
      cfg.i2c_port = 1;
      cfg.i2c_addr = 0x38;
      cfg.pin_sda = 18;
      cfg.pin_scl = 19;
      cfg.freq = 400000;

      _touch_instance.config(cfg);
      _panel_instance.setTouch(&_touch_instance);
    }

    setPanel(&_panel_instance);
  }
};

#endif // CONFIG_H
