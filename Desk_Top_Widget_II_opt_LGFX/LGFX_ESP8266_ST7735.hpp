// LovyanGFX Konfiguration für ESP8266 mit ST7735 Display
// Pin-Belegung: CS=15, DC=2, RST=12

#pragma once

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

class LGFX : public lgfx::LGFX_Device
{
  lgfx::Panel_ST7735S _panel_instance;
  lgfx::Bus_SPI _bus_instance;

public:
  LGFX(void)
  {
    {
      auto cfg = _bus_instance.config();

      cfg.spi_host = VSPI_HOST;     // ESP8266 verwendet HSPI
      cfg.spi_mode = 0;
      cfg.freq_write = 40000000;    // 40MHz beim Schreiben
      cfg.freq_read  = 16000000;    // 16MHz beim Lesen
      cfg.spi_3wire  = false;
      cfg.use_lock   = true;
      cfg.dma_channel = 0;
      cfg.pin_sclk = 14;            // SPI SCLK
      cfg.pin_mosi = 13;            // SPI MOSI
      cfg.pin_miso = 12;            // SPI MISO (nicht verwendet für Display)
      cfg.pin_dc   = 2;             // Data/Command pin

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
      cfg.offset_x         = 0;
      cfg.offset_y         = 0;
      cfg.offset_rotation  = 0;
      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits  = 1;
      cfg.readable         = false;
      cfg.invert           = true;  // ST7735 BLACKTAB benötigt invert
      cfg.rgb_order        = false; // BGR Order für ST7735
      cfg.dlen_16bit       = false;
      cfg.bus_shared       = false;

      _panel_instance.config(cfg);
    }

    setPanel(&_panel_instance);
  }
};
