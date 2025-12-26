// ===================================================================
// TFT_eSPI User Setup for M5Stack AtomS3
// ===================================================================
// Copy this file to your TFT_eSPI library folder:
// C:\Users\<username>\Documents\Arduino\libraries\TFT_eSPI\User_Setup.h
//
// Or include it in User_Setup_Select.h by uncommenting the appropriate line
// ===================================================================

#define USER_SETUP_INFO "M5Stack_AtomS3"

// Driver
#define GC9107_DRIVER

// Display size
#define TFT_WIDTH  128
#define TFT_HEIGHT 128

// Pin definitions for M5Stack AtomS3
#define TFT_MOSI 21
#define TFT_SCLK 17
#define TFT_CS   15
#define TFT_DC   33
#define TFT_RST  34
#define TFT_BL   16  // Backlight

// Backlight control (optional)
// Uncomment if you want software backlight control
// #define TFT_BACKLIGHT_ON HIGH

// SPI frequency
#define SPI_FREQUENCY  40000000  // 40MHz
#define SPI_READ_FREQUENCY  20000000  // 20MHz

// Color format
#define TFT_RGB_ORDER TFT_BGR  // Colour order Blue-Green-Red

// Font configuration
#define LOAD_GLCD   // Font 1. Original Adafruit 8 pixel font needs ~1820 bytes in FLASH
#define LOAD_FONT2  // Font 2. Small 16 pixel high font, needs ~3534 bytes in FLASH, 96 characters
#define LOAD_FONT4  // Font 4. Medium 26 pixel high font, needs ~5848 bytes in FLASH, 96 characters
#define LOAD_FONT6  // Font 6. Large 48 pixel font, needs ~2666 bytes in FLASH, only characters 1234567890:-.apm
#define LOAD_FONT7  // Font 7. 7 segment 48 pixel font, needs ~2438 bytes in FLASH, only characters 1234567890:.
#define LOAD_FONT8  // Font 8. Large 75 pixel font needs ~3256 bytes in FLASH, only characters 1234567890:-.
#define LOAD_GFXFF  // FreeFonts. Include access to the 48 Adafruit_GFX free fonts FF1 to FF48 and custom fonts

#define SMOOTH_FONT

// SPI port
// Defaults to VSPI (ESP32 has HSPI and VSPI)
// #define USE_HSPI_PORT

// DMA (Direct Memory Access) for faster screen updates
// Uncomment for better performance
// #define DMA_CHANNEL 1
