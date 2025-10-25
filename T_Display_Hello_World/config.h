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
      cfg.spi_mode = 3;          // ST7789: Mode 3 funktioniert zuverlässig
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
      cfg.panel_width = 135;
      cfg.panel_height = 240;

      // T-Display 1.14" ist ein 240x240-ST7789 mit Fenster 135x240
      // Offsets werden in setRotation() gesetzt
      cfg.offset_x = 0;
      cfg.offset_y = 0;

      cfg.invert = true;    // ST7789 benötigt invertierte Farben
      cfg.rgb_order = false;
      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits = 1;
      cfg.readable = false;
      cfg.bus_shared = true;

      // Memory-Scan Richtung (das Panel ist eigentlich 240x240)
      cfg.memory_width = 240;
      cfg.memory_height = 240;

      _panel_instance.config(cfg);
    }

    {
      // Backlight Konfiguration (PWM)
      auto cfg = _light_instance.config();

      cfg.pin_bl = 4;        // Backlight Pin beim T-Display
      cfg.freq = 44100;      // PWM-Frequenz
      cfg.pwm_channel = 7;   // nutze freien Kanal

      _light_instance.config(cfg);
      _panel_instance.setLight(&_light_instance);
    }

    setPanel(&_panel_instance);
  }

  // setRotation überschreiben, um Offsets für jede Rotation anzupassen
  void setRotation(uint8_t rotation)
  {
    // Offsets für verschiedene Rotationen
    // rotation 0 = Portrait, 1 = Landscape, 2 = Portrait 180°, 3 = Landscape 180°

    rotation &= 0x03;
    lgfx::LGFX_Device::setRotation(rotation); // Basis-Rotation setzen

    // Dann Offsets anpassen
    auto cfg = _panel_instance.config();
    switch (rotation) {
      case 0: // Portrait
        cfg.offset_x = 52;
        cfg.offset_y = 40;
        break;
      case 1: // Landscape (funktioniert mit diesen Werten)
        cfg.offset_x = 52;
        cfg.offset_y = 40;
        break;
      case 2: // Portrait 180°
        cfg.offset_x = 52;
        cfg.offset_y = 40;
        break;
      case 3: // Landscape 180° (invertierte Offsets)
        cfg.offset_x = 40;
        cfg.offset_y = 52;
        break;
    }
    _panel_instance.config(cfg);
  }
};

#endif // CONFIG_H
