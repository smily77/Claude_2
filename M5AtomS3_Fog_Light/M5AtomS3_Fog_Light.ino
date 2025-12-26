/*
 * M5Stack AtomS3 - Nebelschlussleuchtensymbol mit 8 Encoder Steuerung
 *
 * Zeigt das originale Nebelschlussleuchtensymbol als 1-Bit Bitmap an.
 * Symbol und Hintergrund werden separat über M5Stack Unit 8 Encoder gesteuert.
 *
 * Hardware:
 * - M5Stack AtomS3 mit 128x128 Display
 * - M5Stack Unit 8 Encoder an Port.A (Grove)
 *
 * Steuerung:
 * - CH1 (Encoder 1): Symbol-Farbton (Hue) 0-360° (0.5° pro Schritt)
 * - CH2 (Encoder 2): Symbol-Helligkeit 0-100% (2% pro Schritt)
 * - CH3 (Encoder 3): Hintergrund-Farbton (Hue) 0-360° (1° pro Schritt)
 * - CH4 (Encoder 4): Hintergrund-Helligkeit 0-100% (2.5% pro Schritt)
 * - Startposition: Symbol Blau (240°) 100%, Hintergrund Schwarz (0°) 0%
 *
 * Voraussetzungen:
 * - TFT_eSPI Bibliothek installiert und konfiguriert für AtomS3
 * - M5Unit-8Encoder Bibliothek installiert
 * - Board: M5Stack-ATOMS3
 *
 * TFT_eSPI Konfiguration (User_Setup.h oder User_Setup_Select.h):
 * - Driver: GC9107
 * - Pins siehe Setup-Kommentare unten
 */

#include <TFT_eSPI.h>
#include <Wire.h>
#include "UNIT_8ENCODER.h"
#include "fog_light_bitmap.h"

// TFT Display
TFT_eSPI tft = TFT_eSPI();

// AtomS3 Display Pins (falls nicht in User_Setup.h definiert)
// TFT_MOSI = 21
// TFT_SCLK = 17
// TFT_CS   = 15
// TFT_DC   = 33
// TFT_RST  = 34
// TFT_BL   = 16 (Backlight)

// Unit 8 Encoder
UNIT_8ENCODER encoder;
#define ENCODER_ADDR 0x41

// Port.A GPIO Pins für AtomS3 (Standard Grove Port)
// AtomS3 Port.A: GPIO2 (SDA), GPIO1 (SCL)
#define SDA_PIN 2
#define SCL_PIN 1

// Bitmap-Eigenschaften
const int BITMAP_WIDTH = 96;
const int BITMAP_HEIGHT = 96;
const int BYTES_PER_ROW = 12;  // (96 + 7) / 8 = 12

// Encoder Schrittweiten
const float SYMBOL_HUE_STEP = 0.5;        // 0.5° pro Schritt
const float SYMBOL_BRIGHTNESS_STEP = 2.0;  // 2% pro Schritt
const float BG_HUE_STEP = 1.0;             // 1° pro Schritt
const float BG_BRIGHTNESS_STEP = 2.5;      // 2.5% pro Schritt

// Symbol HSV Einstellungen
float symbolHue = 240.0;          // Start: Blau (240°)
float symbolBrightness = 100.0;   // Start: 100% Helligkeit

// Hintergrund HSV Einstellungen
float bgHue = 0.0;                // Start: Rot (0°)
float bgBrightness = 0.0;         // Start: 0% (Schwarz)

// Encoder-Tracking
int32_t lastEncoderValue_CH1 = 0;
int32_t lastEncoderValue_CH2 = 0;
int32_t lastEncoderValue_CH3 = 0;
int32_t lastEncoderValue_CH4 = 0;

// Update-Flag
bool needsRedraw = true;

// I2C Status
bool encoderConnected = false;

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\n=== M5Stack AtomS3 - Fog Light (TFT_eSPI) ===");

  // TFT Display initialisieren
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);

  Serial.println("TFT Display initialized");
  Serial.printf("Display Size: %dx%d\n", tft.width(), tft.height());

  // I2C Scanner durchführen
  Serial.println("\nScanning I2C Bus...");
  Wire.begin(SDA_PIN, SCL_PIN, 100000UL);  // Explizit mit Pins und Frequenz
  delay(100);

  scanI2C();

  // Unit 8 Encoder initialisieren
  Serial.println("\nInitializing Unit 8 Encoder...");

  // Versuche Encoder zu initialisieren
  if (encoder.begin(&Wire, ENCODER_ADDR, SDA_PIN, SCL_PIN, 100000UL)) {
    Serial.println("✓ Encoder initialized successfully");
    encoderConnected = true;

    // Encoder-Werte initialisieren
    delay(50);
    lastEncoderValue_CH1 = encoder.getEncoderValue(0);  // CH1
    lastEncoderValue_CH2 = encoder.getEncoderValue(1);  // CH2
    lastEncoderValue_CH3 = encoder.getEncoderValue(2);  // CH3
    lastEncoderValue_CH4 = encoder.getEncoderValue(3);  // CH4

    Serial.printf("Initial CH1: %d, CH2: %d, CH3: %d, CH4: %d\n",
                  lastEncoderValue_CH1, lastEncoderValue_CH2,
                  lastEncoderValue_CH3, lastEncoderValue_CH4);
  } else {
    Serial.println("✗ Encoder initialization failed!");
    encoderConnected = false;
  }

  // Erstes Symbol zeichnen
  displayFogLight((int)symbolHue, (int)symbolBrightness,
                  (int)bgHue, (int)bgBrightness,
                  fog_light_icon_96x96);

  Serial.println("\n=== Setup Complete ===");
  Serial.println("CH1: Symbol Hue (0-360°, 0.5° steps)");
  Serial.println("CH2: Symbol Brightness (0-100%, 2% steps)");
  Serial.println("CH3: Background Hue (0-360°, 1° steps)");
  Serial.println("CH4: Background Brightness (0-100%, 2.5% steps)");
  Serial.printf("Start - Symbol: Hue=%d° Bright=%d%%  BG: Hue=%d° Bright=%d%%\n",
                (int)symbolHue, (int)symbolBrightness,
                (int)bgHue, (int)bgBrightness);
}

void loop() {
  if (!encoderConnected) {
    // Periodisch versuchen, Encoder neu zu verbinden
    static unsigned long lastRetry = 0;
    if (millis() - lastRetry > 5000) {
      lastRetry = millis();
      Serial.println("Retrying encoder connection...");
      if (encoder.begin(&Wire, ENCODER_ADDR, SDA_PIN, SCL_PIN, 100000UL)) {
        encoderConnected = true;
        Serial.println("✓ Encoder reconnected!");
      }
    }
    delay(100);
    return;
  }

  // CH1: Symbol Farbton (Hue) - Encoder 0
  int32_t encoderValue_CH1 = encoder.getEncoderValue(0);
  if (encoderValue_CH1 != lastEncoderValue_CH1) {
    int32_t delta = encoderValue_CH1 - lastEncoderValue_CH1;

    // Hue ändern (0.5° pro Encoder-Schritt)
    symbolHue += (delta * SYMBOL_HUE_STEP);

    // Hue im Bereich 0-360° halten (Wraparound)
    while (symbolHue < 0) symbolHue += 360.0;
    while (symbolHue >= 360.0) symbolHue -= 360.0;

    lastEncoderValue_CH1 = encoderValue_CH1;
    needsRedraw = true;

    Serial.printf("CH1: Symbol Hue = %.1f°\n", symbolHue);
  }

  // CH2: Symbol Helligkeit - Encoder 1
  int32_t encoderValue_CH2 = encoder.getEncoderValue(1);
  if (encoderValue_CH2 != lastEncoderValue_CH2) {
    int32_t delta = encoderValue_CH2 - lastEncoderValue_CH2;

    // Helligkeit ändern (2% pro Encoder-Schritt)
    symbolBrightness += (delta * SYMBOL_BRIGHTNESS_STEP);

    // Helligkeit begrenzen (0-100%)
    if (symbolBrightness > 100.0) symbolBrightness = 100.0;
    if (symbolBrightness < 0.0) symbolBrightness = 0.0;

    lastEncoderValue_CH2 = encoderValue_CH2;
    needsRedraw = true;

    Serial.printf("CH2: Symbol Brightness = %.1f%%\n", symbolBrightness);
  }

  // CH3: Hintergrund Farbton (Hue) - Encoder 2
  int32_t encoderValue_CH3 = encoder.getEncoderValue(2);
  if (encoderValue_CH3 != lastEncoderValue_CH3) {
    int32_t delta = encoderValue_CH3 - lastEncoderValue_CH3;

    // Hue ändern (1° pro Encoder-Schritt)
    bgHue += (delta * BG_HUE_STEP);

    // Hue im Bereich 0-360° halten (Wraparound)
    while (bgHue < 0) bgHue += 360.0;
    while (bgHue >= 360.0) bgHue -= 360.0;

    lastEncoderValue_CH3 = encoderValue_CH3;
    needsRedraw = true;

    Serial.printf("CH3: Background Hue = %.1f°\n", bgHue);
  }

  // CH4: Hintergrund Helligkeit - Encoder 3
  int32_t encoderValue_CH4 = encoder.getEncoderValue(3);
  if (encoderValue_CH4 != lastEncoderValue_CH4) {
    int32_t delta = encoderValue_CH4 - lastEncoderValue_CH4;

    // Helligkeit ändern (2.5% pro Encoder-Schritt)
    bgBrightness += (delta * BG_BRIGHTNESS_STEP);

    // Helligkeit begrenzen (0-100%)
    if (bgBrightness > 100.0) bgBrightness = 100.0;
    if (bgBrightness < 0.0) bgBrightness = 0.0;

    lastEncoderValue_CH4 = encoderValue_CH4;
    needsRedraw = true;

    Serial.printf("CH4: Background Brightness = %.1f%%\n", bgBrightness);
  }

  // Neuzeichnen wenn nötig
  if (needsRedraw) {
    displayFogLight((int)symbolHue, (int)symbolBrightness,
                    (int)bgHue, (int)bgBrightness,
                    fog_light_icon_96x96);
    needsRedraw = false;
  }

  delay(10);
}

/**
 * Zeigt das Nebelschlussleuchtensymbol mit individuellen Farben für Symbol und Hintergrund
 *
 * @param symbolHue         Farbton des Symbols (0-360°)
 * @param symbolBrightness  Helligkeit des Symbols (0-100%)
 * @param bgHue            Farbton des Hintergrundes (0-360°)
 * @param bgBrightness     Helligkeit des Hintergrundes (0-100%)
 * @param bitmap           Zeiger auf das 1-Bit Bitmap-Array
 */
void displayFogLight(int symbolHue, int symbolBrightness,
                     int bgHue, int bgBrightness,
                     const unsigned char* bitmap) {

  // Farben berechnen
  uint16_t symbolColor = hsvToRgb565(symbolHue, 100, symbolBrightness);
  uint16_t bgColor = hsvToRgb565(bgHue, 100, bgBrightness);

  // Hintergrund mit gewählter Farbe füllen
  tft.fillScreen(bgColor);

  // Bitmap zentriert auf dem 128x128 Display zeichnen
  int offsetX = (tft.width() - BITMAP_WIDTH) / 2;
  int offsetY = (tft.height() - BITMAP_HEIGHT) / 2;

  // Jedes Pixel des Bitmaps durchgehen
  for (int y = 0; y < BITMAP_HEIGHT; y++) {
    for (int x = 0; x < BITMAP_WIDTH; x++) {
      // Byte-Index und Bit-Position berechnen
      int byteIndex = y * BYTES_PER_ROW + (x / 8);
      int bitPosition = 7 - (x % 8);

      // Bit auslesen (0 = Symbol zeichnen, 1 = Hintergrund)
      bool isBackground = (bitmap[byteIndex] >> bitPosition) & 0x01;

      if (!isBackground) {
        // Symbol-Pixel zeichnen
        tft.drawPixel(offsetX + x, offsetY + y, symbolColor);
      }
      // Hintergrund-Pixel wurden bereits durch fillScreen gesetzt
    }
  }
}

/**
 * HSV zu RGB565 Konvertierung
 *
 * @param h Hue (0-360°)
 * @param s Saturation (0-100%)
 * @param v Value/Brightness (0-100%)
 * @return RGB565 Farbwert
 */
uint16_t hsvToRgb565(int h, int s, int v) {
  float H = h;
  float S = s / 100.0;
  float V = v / 100.0;

  float C = V * S;
  float X = C * (1 - abs(fmod(H / 60.0, 2) - 1));
  float m = V - C;

  float r, g, b;

  if (H >= 0 && H < 60) {
    r = C; g = X; b = 0;
  } else if (H >= 60 && H < 120) {
    r = X; g = C; b = 0;
  } else if (H >= 120 && H < 180) {
    r = 0; g = C; b = X;
  } else if (H >= 180 && H < 240) {
    r = 0; g = X; b = C;
  } else if (H >= 240 && H < 300) {
    r = X; g = 0; b = C;
  } else {
    r = C; g = 0; b = X;
  }

  // Zu 0-255 konvertieren
  uint8_t R = (r + m) * 255;
  uint8_t G = (g + m) * 255;
  uint8_t B = (b + m) * 255;

  // Zu RGB565 konvertieren
  uint16_t r5 = (R >> 3) & 0x1F;
  uint16_t g6 = (G >> 2) & 0x3F;
  uint16_t b5 = (B >> 3) & 0x1F;

  return (r5 << 11) | (g6 << 5) | b5;
}

/**
 * Scannt den I2C Bus nach angeschlossenen Geräten
 */
void scanI2C() {
  byte error, address;
  int deviceCount = 0;

  Serial.println("Scanning I2C addresses 0x01-0x7F...");

  for (address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0) {
      Serial.printf("✓ I2C device found at 0x%02X\n", address);
      deviceCount++;
    }
  }

  if (deviceCount == 0) {
    Serial.println("✗ No I2C devices found!");
    Serial.println("Check connections:");
    Serial.printf("  SDA: GPIO%d\n", SDA_PIN);
    Serial.printf("  SCL: GPIO%d\n", SCL_PIN);
  } else {
    Serial.printf("✓ Found %d device(s) on I2C bus\n", deviceCount);
  }
}
