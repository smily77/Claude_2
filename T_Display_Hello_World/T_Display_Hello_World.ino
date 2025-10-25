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
  tft.setRotation(1);        // Landscape-Modus (240x135)
  tft.setBrightness(255);    // Backlight auf Maximum (0-255)
  tft.fillScreen(COLOR_BG);

  // Header-Bereich zeichnen
  tft.fillRect(0, 0, 240, 30, COLOR_HEADER);
  tft.drawLine(0, 30, 240, 30, COLOR_TEXT1);

  // "Hallo Welt!" Haupttext
  tft.setTextColor(COLOR_TEXT1, COLOR_BG);
  tft.setTextSize(3);
  tft.setCursor(20, 50);
  tft.println("Hallo Welt!");

  // Gerätename
  tft.setTextColor(COLOR_TEXT2, COLOR_BG);
  tft.setTextSize(2);
  tft.setCursor(25, 85);
  tft.println("LILYGO T-Display");

  // Info-Text
  tft.setTextColor(COLOR_TEXT3, COLOR_BG);
  tft.setTextSize(1);
  tft.setCursor(45, 110);
  tft.println("ESP32 mit ST7789");

  // Anweisungen
  tft.setTextColor(TFT_WHITE, COLOR_BG);
  tft.setTextSize(1);
  tft.setCursor(20, 125);
  tft.println("Druecke Buttons zum Testen");

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
  // Counter-Bereich löschen
  tft.fillRect(0, 0, 240, 30, COLOR_HEADER);

  // Counter anzeigen
  tft.setTextColor(TFT_WHITE, COLOR_HEADER);
  tft.setTextSize(2);
  tft.setCursor(10, 8);
  tft.print("Zaehler: ");
  tft.print(counter);

  Serial.print("Counter: ");
  Serial.println(counter);
}
