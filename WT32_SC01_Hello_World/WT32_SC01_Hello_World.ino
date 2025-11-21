/*
 * WT32-SC01 - Hallo Welt mit Touch
 *
 * Ein einfaches Beispiel für das WT32-SC01 3.5" Display.
 * Zeigt Text, Grafik und Touch-Funktionalität.
 *
 * Bibliotheken:
 * - LovyanGFX (einfach über Bibliotheksverwalter installieren)
 *
 * Hardware:
 * - WT32-SC01
 * - Display: 3.5" TFT LCD (480x320 Pixel)
 * - Controller: ST7796
 * - Touch: FT6336U (Capacitive Touch)
 * - MCU: ESP32-WROVER
 *
 * Vorteil LovyanGFX:
 * - Keine Änderung von Library-Dateien nötig!
 * - Alle Einstellungen in config.h im gleichen Ordner
 * - Schneller und flexibler als TFT_eSPI
 * - Touch-Unterstützung integriert
 */

#include "config.h"

// Display Objekt erstellen
static LGFX tft;

// Farben (RGB565 Format)
#define COLOR_BG     TFT_BLACK      // Schwarz
#define COLOR_HEADER TFT_NAVY       // Dunkelblau
#define COLOR_TEXT1  TFT_CYAN       // Cyan
#define COLOR_TEXT2  TFT_YELLOW     // Gelb
#define COLOR_TEXT3  TFT_GREEN      // Grün
#define COLOR_BUTTON TFT_DARKGREY   // Dunkelgrau
#define COLOR_TOUCH  TFT_RED        // Rot für Touch-Punkt

// Button-Bereich definieren
struct Button {
  int x, y, w, h;
  const char* label;
  uint16_t color;
};

Button buttons[] = {
  {20, 230, 100, 60, "Rot", TFT_RED},
  {140, 230, 100, 60, "Gruen", TFT_GREEN},
  {260, 230, 100, 60, "Blau", TFT_BLUE},
  {380, 230, 80, 60, "Reset", TFT_DARKGREY}
};

int counter = 0;
uint16_t bgColor = COLOR_BG;

void setup() {
  // Serielle Kommunikation starten
  Serial.begin(115200);
  Serial.println("WT32-SC01 Hello World startet...");

  // Display initialisieren
  tft.init();
  tft.setRotation(1);        // Landscape-Modus (480x320)
  tft.setBrightness(255);    // Backlight auf Maximum (0-255)

  // Ersten Screen zeichnen
  drawMainScreen();

  Serial.println("Display und Touch bereit!");
  Serial.println("Beruehre die Buttons am unteren Rand!");
}

void loop() {
  // Touch-Status prüfen
  uint16_t t_x = 0, t_y = 0;
  bool touched = tft.getTouch(&t_x, &t_y);

  if (touched) {
    Serial.print("Touch erkannt bei X:");
    Serial.print(t_x);
    Serial.print(" Y:");
    Serial.println(t_y);

    // Touch-Punkt anzeigen (kurz)
    tft.fillCircle(t_x, t_y, 5, COLOR_TOUCH);
    delay(100);

    // Prüfen, welcher Button gedrückt wurde
    checkButtons(t_x, t_y);

    // Warten bis Touch losgelassen wird
    while (tft.getTouch(&t_x, &t_y)) {
      delay(10);
    }

    delay(100); // Debounce
  }

  delay(50);
}

void drawMainScreen() {
  tft.fillScreen(bgColor);

  // Gradient-Header zeichnen
  for (int i = 0; i < 50; i++) {
    uint16_t color = tft.color565(0, 0, 128 - i * 2);  // Blau-Gradient
    tft.drawFastHLine(0, i, 480, color);
  }
  tft.drawFastHLine(0, 50, 480, COLOR_TEXT1);  // Cyan Trennlinie

  // "Hallo Welt!" - Großer fetter Titel
  tft.setFont(&fonts::FreeSansBold24pt7b);  // Große fette Schrift
  tft.setTextColor(COLOR_TEXT1);
  tft.setTextDatum(top_center);
  tft.drawString("Hallo Welt!", 240, 60);

  // Gerätename
  tft.setFont(&fonts::FreeSerif18pt7b);
  tft.setTextColor(COLOR_TEXT2);
  tft.setTextDatum(top_center);
  tft.drawString("WT32-SC01", 240, 110);

  // Info-Text
  tft.setFont(&fonts::FreeSans12pt7b);
  tft.setTextColor(COLOR_TEXT3);
  tft.setTextDatum(top_center);
  tft.drawString("3.5\" Display mit Touch", 240, 150);

  // Counter anzeigen
  tft.setFont(&fonts::FreeSansBold12pt7b);
  tft.setTextColor(TFT_WHITE);
  tft.setTextDatum(top_center);
  String counterText = "Zaehler: " + String(counter);
  tft.drawString(counterText, 240, 180);

  // Buttons zeichnen
  drawButtons();
}

void drawButtons() {
  for (int i = 0; i < 4; i++) {
    // Button-Hintergrund
    tft.fillRoundRect(buttons[i].x, buttons[i].y, buttons[i].w, buttons[i].h, 8, buttons[i].color);

    // Button-Rahmen
    tft.drawRoundRect(buttons[i].x, buttons[i].y, buttons[i].w, buttons[i].h, 8, TFT_WHITE);

    // Button-Text
    tft.setFont(&fonts::FreeSans9pt7b);
    tft.setTextColor(TFT_WHITE);
    tft.setTextDatum(middle_center);
    tft.drawString(buttons[i].label,
                   buttons[i].x + buttons[i].w / 2,
                   buttons[i].y + buttons[i].h / 2);
  }
}

void checkButtons(uint16_t x, uint16_t y) {
  for (int i = 0; i < 4; i++) {
    // Prüfen, ob Touch innerhalb des Buttons liegt
    if (x >= buttons[i].x && x <= (buttons[i].x + buttons[i].w) &&
        y >= buttons[i].y && y <= (buttons[i].y + buttons[i].h)) {

      Serial.print("Button gedrueckt: ");
      Serial.println(buttons[i].label);

      // Button-Aktion
      if (i == 0) {
        // Rot
        bgColor = TFT_DARKRED;
        counter++;
      } else if (i == 1) {
        // Grün
        bgColor = TFT_DARKGREEN;
        counter++;
      } else if (i == 2) {
        // Blau
        bgColor = TFT_DARKBLUE;
        counter++;
      } else if (i == 3) {
        // Reset
        bgColor = COLOR_BG;
        counter = 0;
      }

      // Screen neu zeichnen
      drawMainScreen();
      break;
    }
  }
}
