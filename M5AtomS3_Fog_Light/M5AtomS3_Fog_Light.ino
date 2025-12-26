/*
 * M5Stack AtomS3 - Nebelschlussleuchtensymbol mit 8 Encoder Steuerung
 *
 * Zeigt das originale Nebelschlussleuchtensymbol als 1-Bit Bitmap an.
 * Farbe und Helligkeit werden über M5Stack Unit 8 Encoder gesteuert.
 *
 * Hardware:
 * - M5Stack AtomS3 mit 128x128 Display
 * - M5Stack Unit 8 Encoder an Port.A (Grove)
 *
 * Steuerung:
 * - CH1 (Encoder 1): Farbwechsel durch Drehen
 * - CH2 (Encoder 2): Helligkeitsänderung durch Drehen
 * - Startposition: Blau, volle Helligkeit
 *
 * Voraussetzungen:
 * - M5Unified Bibliothek installiert
 * - M5Unit-8Encoder Bibliothek installiert
 * - Board: M5Stack-ATOMS3
 */

#include <M5Unified.h>
#include "UNIT_8ENCODER.h"
#include "fog_light_bitmap.h"

// Unit 8 Encoder
UNIT_8ENCODER encoder;
#define ENCODER_ADDR 0x41

// Port.A GPIO Pins für AtomS3
#define SDA_PIN 1
#define SCL_PIN 2

// Farbpalette für das Symbol
uint16_t baseColors[] = {
  TFT_RED,
  TFT_ORANGE,
  TFT_YELLOW,
  TFT_GREEN,
  TFT_CYAN,
  TFT_BLUE,      // Index 5 - Startfarbe
  TFT_MAGENTA,
  TFT_WHITE
};

const int numColors = sizeof(baseColors) / sizeof(baseColors[0]);

// Bitmap-Eigenschaften
const int BITMAP_WIDTH = 96;
const int BITMAP_HEIGHT = 96;
const int BYTES_PER_ROW = 12;  // (96 + 7) / 8 = 12

// Aktuelle Einstellungen
int currentColorIndex = 5;     // Start: Blau (Index 5)
int currentBrightness = 100;   // Start: 100% Helligkeit

// Encoder-Tracking
int32_t lastEncoderValue_CH1 = 0;
int32_t lastEncoderValue_CH2 = 0;

// Update-Flag
bool needsRedraw = true;

void setup() {
  // M5Unified initialisieren
  auto cfg = M5.config();
  M5.begin(cfg);

  // Display initialisieren
  M5.Display.setRotation(0);
  M5.Display.fillScreen(TFT_BLACK);

  // I2C für Unit 8 Encoder initialisieren
  Wire.begin(SDA_PIN, SCL_PIN);

  // Unit 8 Encoder initialisieren
  encoder.begin(&Wire, ENCODER_ADDR, SDA_PIN, SCL_PIN, 100000UL);

  // Encoder-Werte initialisieren
  lastEncoderValue_CH1 = encoder.getEncoderValue(0);  // CH1
  lastEncoderValue_CH2 = encoder.getEncoderValue(1);  // CH2

  // Erstes Symbol zeichnen
  drawFogLightBitmap();

  Serial.begin(115200);
  Serial.println("M5Stack AtomS3 - Fog Light mit 8 Encoder");
  Serial.println("CH1: Farbe, CH2: Helligkeit");
}

void loop() {
  // M5Stack aktualisieren
  M5.update();

  // CH1: Farbwechsel (Encoder 0)
  int32_t encoderValue_CH1 = encoder.getEncoderValue(0);
  if (encoderValue_CH1 != lastEncoderValue_CH1) {
    int32_t delta = encoderValue_CH1 - lastEncoderValue_CH1;

    // Farbe ändern basierend auf Encoder-Richtung
    if (delta > 0) {
      // Rechts gedreht: nächste Farbe
      currentColorIndex = (currentColorIndex + 1) % numColors;
    } else if (delta < 0) {
      // Links gedreht: vorherige Farbe
      currentColorIndex = (currentColorIndex - 1 + numColors) % numColors;
    }

    lastEncoderValue_CH1 = encoderValue_CH1;
    needsRedraw = true;

    Serial.printf("CH1: Farbe = %d\n", currentColorIndex);
  }

  // CH2: Helligkeit (Encoder 1)
  int32_t encoderValue_CH2 = encoder.getEncoderValue(1);
  if (encoderValue_CH2 != lastEncoderValue_CH2) {
    int32_t delta = encoderValue_CH2 - lastEncoderValue_CH2;

    // Helligkeit ändern basierend auf Encoder-Richtung
    currentBrightness += (delta > 0) ? 5 : -5;

    // Helligkeit begrenzen (0-100%)
    if (currentBrightness > 100) currentBrightness = 100;
    if (currentBrightness < 0) currentBrightness = 0;

    lastEncoderValue_CH2 = encoderValue_CH2;
    needsRedraw = true;

    Serial.printf("CH2: Helligkeit = %d%%\n", currentBrightness);
  }

  // Neuzeichnen wenn nötig
  if (needsRedraw) {
    M5.Display.fillScreen(TFT_BLACK);
    drawFogLightBitmap();
    needsRedraw = false;
  }

  delay(10);
}

void drawFogLightBitmap() {
  // Aktuelle Farbe mit Helligkeit berechnen
  uint16_t displayColor = adjustBrightness(baseColors[currentColorIndex], currentBrightness);

  // Bitmap zentriert auf dem 128x128 Display zeichnen
  int offsetX = (128 - BITMAP_WIDTH) / 2;
  int offsetY = (128 - BITMAP_HEIGHT) / 2;

  // Jedes Pixel des Bitmaps durchgehen
  for (int y = 0; y < BITMAP_HEIGHT; y++) {
    for (int x = 0; x < BITMAP_WIDTH; x++) {
      // Byte-Index und Bit-Position berechnen
      int byteIndex = y * BYTES_PER_ROW + (x / 8);
      int bitPosition = 7 - (x % 8);

      // Bit auslesen (0 = Symbol zeichnen)
      bool pixelSet = (fog_light_icon_96x96[byteIndex] >> bitPosition) & 0x01;

      if (!pixelSet) {
        M5.Display.drawPixel(offsetX + x, offsetY + y, displayColor);
      }
    }
  }
}

uint16_t adjustBrightness(uint16_t color, int brightness) {
  // RGB565 Farbe in Komponenten zerlegen
  uint8_t r = (color >> 11) & 0x1F;
  uint8_t g = (color >> 5) & 0x3F;
  uint8_t b = color & 0x1F;

  // Helligkeit anwenden (0-100%)
  r = (r * brightness) / 100;
  g = (g * brightness) / 100;
  b = (b * brightness) / 100;

  // Zurück in RGB565 konvertieren
  return (r << 11) | (g << 5) | b;
}
