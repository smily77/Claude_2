/*
 * LILYGO T-Display (ESP32) - Hallo Welt
 *
 * Ein einfaches Hello World Programm für das LILYGO T-Display.
 * Das T-Display hat ein 135x240 Pixel TFT-Display und 2 Buttons.
 *
 * Bibliotheken:
 * - LovyanGFX (einfach über Bibliotheksverwalter installieren)
 *
 * Hardware:
 * - LILYGO T-Display (ESP32)
 * - Display: ST7789 135x240 Pixel
 * - 2 Buttons (GPIO 0 und GPIO 35)
 *
 * Vorteil LovyanGFX:
 * - Keine Änderung von Library-Dateien nötig!
 * - Alle Einstellungen in config.h im gleichen Ordner
 * - Schneller und flexibler als TFT_eSPI
 */

#include "config.h"

// Display Objekt erstellen
static LGFX tft;

// Button Pins
#define BUTTON_LEFT  0
#define BUTTON_RIGHT 35

// Farben (RGB565 Format)
#define COLOR_BG     TFT_BLACK     // Schwarz
#define COLOR_HEADER TFT_NAVY      // Dunkelblau
#define COLOR_TEXT1  TFT_CYAN      // Cyan
#define COLOR_TEXT2  TFT_YELLOW    // Gelb
#define COLOR_TEXT3  TFT_GREEN     // Grün

void setup() {
  // Serielle Kommunikation starten
  Serial.begin(115200);
  Serial.println("T-Display Hello World startet...");

  // Buttons als Eingänge konfigurieren
  pinMode(BUTTON_LEFT, INPUT);
  pinMode(BUTTON_RIGHT, INPUT);

  // Display initialisieren
  tft.init();
  tft.setRotation(3);        // Landscape-Modus (240x135)
  tft.setBrightness(255);    // Backlight auf Maximum (0-255)
  tft.fillScreen(COLOR_BG);

  // Gradient-Header (schöner als einfarbig)
  for (int i = 0; i < 35; i++) {
    uint16_t color = tft.color565(0, 0, 128 - i * 2);  // Blau-Gradient
    tft.drawFastHLine(0, i, 240, color);
  }
  tft.drawFastHLine(0, 35, 240, COLOR_TEXT1);  // Cyan Trennlinie

  // "Hallo Welt!" - Großer fetter Titel
  tft.setFont(&fonts::FreeSansBold18pt7b);  // Große fette Schrift
  tft.setTextColor(COLOR_TEXT1);
  tft.setTextDatum(top_center);              // Zentriert
  tft.drawString("Hallo Welt!", 120, 45);

  // Gerätename - Elegante Schrift
  tft.setFont(&fonts::FreeSerif12pt7b);      // Serif = eleganter
  tft.setTextColor(COLOR_TEXT2);
  tft.setTextDatum(top_center);
  tft.drawString("LILYGO T-Display", 120, 80);

  // Info-Text - Kleinere Sans-Serif
  tft.setFont(&fonts::FreeSans9pt7b);
  tft.setTextColor(COLOR_TEXT3);
  tft.setTextDatum(top_center);
  tft.drawString("ESP32 mit ST7789", 120, 105);

  // Anweisungen - Noch kleiner
  tft.setFont(&fonts::Font0);                // Kleine System-Schrift
  tft.setTextColor(TFT_LIGHTGREY);
  tft.setTextDatum(top_center);
  tft.drawString("Buttons: Links/Rechts", 120, 123);

  Serial.println("Display bereit!");
}

int counter = 0;

void loop() {
  // Linker Button gedrückt?
  if (digitalRead(BUTTON_LEFT) == LOW) {
    counter--;
    updateCounter();
    delay(200);  // Debounce
  }

  // Rechter Button gedrückt?
  if (digitalRead(BUTTON_RIGHT) == LOW) {
    counter++;
    updateCounter();
    delay(200);  // Debounce
  }

  delay(50);
}

void updateCounter() {
  // Gradient-Header neu zeichnen
  for (int i = 0; i < 35; i++) {
    uint16_t color = tft.color565(0, 0, 128 - i * 2);  // Blau-Gradient
    tft.drawFastHLine(0, i, 240, color);
  }

  // Counter mit schöner Schrift anzeigen
  tft.setFont(&fonts::FreeSansBold12pt7b);
  tft.setTextColor(TFT_WHITE);
  tft.setTextDatum(middle_center);

  String counterText = "Zaehler: " + String(counter);
  tft.drawString(counterText, 120, 17);

  Serial.print("Counter: ");
  Serial.println(counter);
}
