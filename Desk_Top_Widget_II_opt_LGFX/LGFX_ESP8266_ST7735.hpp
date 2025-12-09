// LovyanGFX Konfiguration für ESP8266 mit ST7735 Display
// Pin-Belegung: CS=15, DC=2, RST=12

#pragma once

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

class LGFX : public lgfx::LGFX_Device
{
  lgfx::Panel_ST7735 _panel_instance;  // BLACKTAB verwendet ST7735, nicht ST7735S!
  lgfx::Bus_SPI _bus_instance;

public:
  LGFX(void)
  {
    {
      auto cfg = _bus_instance.config();

      // ESP8266 SPI Konfiguration
      cfg.spi_mode = 0;
      cfg.freq_write = 40000000;    // 40MHz beim Schreiben
      cfg.freq_read  = 16000000;    // 16MHz beim Lesen
      cfg.spi_3wire  = false;
      cfg.pin_sclk = 14;            // SPI SCLK (D5)
      cfg.pin_mosi = 13;            // SPI MOSI (D7)
      cfg.pin_miso = -1;            // MISO nicht verwendet (Pin 12 ist RST!)
      cfg.pin_dc   = 2;             // Data/Command pin (D4)

      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }

    {
      auto cfg = _panel_instance.config();

      cfg.pin_cs           = 15;    // Chip Select
      cfg.pin_rst          = 12;    // Reset
      cfg.pin_busy         = -1;    // Busy pin (nicht verwendet)

      cfg.panel_width      = 128;   // Tatsächliche Breite
      cfg.panel_height     = 160;   // Tatsächliche Höhe
      cfg.offset_x         = 2;     // BLACKTAB Offset
      cfg.offset_y         = 1;     // BLACKTAB Offset
      cfg.offset_rotation  = 0;
      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits  = 1;
      cfg.readable         = false;
      cfg.invert           = false; // BLACKTAB benötigt kein invert
      cfg.rgb_order        = false; // BGR Order für ST7735
      cfg.dlen_16bit       = false;
      cfg.bus_shared       = false;

      _panel_instance.config(cfg);
    }

    setPanel(&_panel_instance);
  }
};
