/*
 * LILYGO T-Display Konfiguration für LovyanGFX
 *
 * Diese Datei enthält alle Display-spezifischen Einstellungen.
 * Keine Änderungen in der Bibliothek notwendig!
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <LovyanGFX.hpp>

// T-Display Konfiguration
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

      cfg.spi_host = VSPI_HOST;  // VSPI_HOST oder HSPI_HOST
      cfg.spi_mode = 0;
      cfg.freq_write = 40000000; // 40MHz
      cfg.freq_read = 16000000;  // 16MHz
      cfg.spi_3wire = true;
      cfg.use_lock = true;
      cfg.dma_channel = 1;

      // T-Display SPI Pins
      cfg.pin_sclk = 18;  // SCLK
      cfg.pin_mosi = 19;  // MOSI
      cfg.pin_miso = -1;  // nicht verwendet
      cfg.pin_dc = 16;    // DC (Data/Command)

      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }

    {
      // Display Panel Konfiguration
      auto cfg = _panel_instance.config();

      cfg.pin_cs = 5;     // Chip Select
      cfg.pin_rst = 23;   // Reset
      cfg.pin_busy = -1;  // nicht verwendet

      // Display Eigenschaften
      cfg.memory_width = 135;
      cfg.memory_height = 240;
      cfg.panel_width = 135;
      cfg.panel_height = 240;
      cfg.offset_x = 52;  // wichtig für T-Display!
      cfg.offset_y = 40;  // wichtig für T-Display!
      cfg.offset_rotation = 2;

      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits = 1;
      cfg.readable = false;
      cfg.invert = true;  // Farben invertiert
      cfg.rgb_order = false;
      cfg.dlen_16bit = false;
      cfg.bus_shared = false;

      _panel_instance.config(cfg);
    }

    {
      // Backlight Konfiguration
      auto cfg = _light_instance.config();

      cfg.pin_bl = 4;        // Backlight Pin
      cfg.invert = false;
      cfg.freq = 44100;
      cfg.pwm_channel = 7;

      _light_instance.config(cfg);
      _panel_instance.setLight(&_light_instance);
    }

    setPanel(&_panel_instance);
  }
};

#endif // CONFIG_H
