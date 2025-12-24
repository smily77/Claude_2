/*
 * M5Stack AtomS3 - Nebelschlussleuchtensymbol
 *
 * Zeigt das originale Nebelschlussleuchtensymbol als 1-Bit Bitmap an.
 * Bei jedem Tastendruck ändert sich die Farbe des Symbols.
 *
 * Hardware:
 * - M5Stack AtomS3 mit 128x128 Display
 * - Eingebauter Button
 *
 * Voraussetzungen:
 * - M5Unified Bibliothek installiert
 * - Board: M5Stack-ATOMS3
 */

#include <M5Unified.h>
#include "fog_light_bitmap.h"

// Farbpalette für das Symbol
uint16_t colors[] = {
  TFT_YELLOW,    // Klassische Nebelleuchten-Farbe
  TFT_ORANGE,
  TFT_RED,
  TFT_GREEN,
  TFT_CYAN,
  TFT_BLUE,
  TFT_MAGENTA,
  TFT_WHITE
};

int currentColorIndex = 0;
const int numColors = sizeof(colors) / sizeof(colors[0]);

// Bitmap-Eigenschaften
const int BITMAP_WIDTH = 96;
const int BITMAP_HEIGHT = 96;
const int BYTES_PER_ROW = 12;  // (96 + 7) / 8 = 12

void setup() {
  // M5Unified initialisieren
  auto cfg = M5.config();
  M5.begin(cfg);

  // Display initialisieren
  M5.Display.setRotation(0);
  M5.Display.fillScreen(TFT_BLACK);

  // Erstes Symbol zeichnen
  drawFogLightBitmap(colors[currentColorIndex]);
}

void loop() {
  // M5Stack aktualisieren
  M5.update();

  // Button-Press erkennen (BtnA für AtomS3)
  if (M5.BtnA.wasPressed()) {
    // Zur nächsten Farbe wechseln
    currentColorIndex = (currentColorIndex + 1) % numColors;

    // Display löschen und Symbol neu zeichnen
    M5.Display.fillScreen(TFT_BLACK);
    drawFogLightBitmap(colors[currentColorIndex]);
  }

  delay(10);
}

void drawFogLightBitmap(uint16_t color) {
  // Bitmap zentriert auf dem 128x128 Display zeichnen
  int offsetX = (128 - BITMAP_WIDTH) / 2;
  int offsetY = (128 - BITMAP_HEIGHT) / 2;

  // Jedes Pixel des Bitmaps durchgehen
  for (int y = 0; y < BITMAP_HEIGHT; y++) {
    for (int x = 0; x < BITMAP_WIDTH; x++) {
      // Byte-Index und Bit-Position berechnen
      int byteIndex = y * BYTES_PER_ROW + (x / 8);
      int bitPosition = 7 - (x % 8);

      // Bit auslesen (1 = Pixel setzen, 0 = überspringen)
      bool pixelSet = (fog_light_icon_96x96[byteIndex] >> bitPosition) & 0x01;

      // Pixel zeichnen wenn gesetzt (1-Bit bedeutet: 1 = weiß/Symbol, 0 = schwarz/Hintergrund)
      // Aber wir haben invertiert, also: 1 = Hintergrund, 0 = Symbol
      // Prüfen wir die Daten: 0xFF = alle Bits gesetzt = Hintergrund
      // Also: 0 = Symbol zeichnen
      if (!pixelSet) {
        M5.Display.drawPixel(offsetX + x, offsetY + y, color);
      }
    }
  }
}
