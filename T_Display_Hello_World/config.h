/*
 * LILYGO T-Display Konfiguration für LovyanGFX
 *
 * Diese Datei enthält alle Display-spezifischen Einstellungen.
 * Keine Änderungen in der Bibliothek notwendig!
 *
 * Basiert auf der offiziellen Konfiguration vom LovyanGFX Maintainer:
 * https://github.com/lovyan03/LovyanGFX/issues/159
 */

#ifndef CONFIG_H
#define CONFIG_H

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

// T-Display Konfiguration (offizielle Version)
class LGFX : public lgfx::LGFX_Device
{
  lgfx::Panel_ST7789 _panel_instance;
  lgfx::Bus_SPI _bus_instance;
  lgfx::Light_PWM _light_instance;

public:
  LGFX(void)
  {
    {
      // SPI Bus Konfiguration
      auto cfg = _bus_instance.config();

      cfg.spi_host = VSPI_HOST;
      cfg.spi_mode = 0;          // Mode 0 laut offizieller Config
      cfg.freq_write = 40000000; // 40MHz
      cfg.freq_read = 14000000;  // 14MHz (laut offizieller Config)
      cfg.spi_3wire = true;
      cfg.use_lock = true;
      cfg.dma_channel = 1;

      // T-Display SPI Pins
      cfg.pin_sclk = 18;
      cfg.pin_mosi = 19;
      cfg.pin_miso = -1;
      cfg.pin_dc = 16;

      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }

    {
      // Backlight Konfiguration
      auto cfg = _light_instance.config();

      cfg.pin_bl = 4;
      cfg.invert = false;
      cfg.freq = 44100;
      cfg.pwm_channel = 7;

      _light_instance.config(cfg);
      _panel_instance.setLight(&_light_instance);
    }

    {
      // Display Panel Konfiguration (offizielle Werte)
      auto cfg = _panel_instance.config();

      cfg.pin_cs = 5;
      cfg.pin_rst = 23;
      cfg.pin_busy = -1;

      cfg.panel_width = 135;
      cfg.panel_height = 240;

      // Feste Offsets - NICHT während Rotation ändern!
      cfg.offset_x = 52;
      cfg.offset_y = 40;
      cfg.offset_rotation = 0;

      cfg.dummy_read_pixel = 16;  // 16 laut offizieller Config (nicht 8)
      cfg.dummy_read_bits = 1;
      cfg.readable = true;         // true laut offizieller Config
      cfg.invert = true;
      cfg.rgb_order = false;
      cfg.dlen_16bit = false;
      cfg.bus_shared = true;

      _panel_instance.config(cfg);
    }

    setPanel(&_panel_instance);
  }
};

#endif // CONFIG_H
