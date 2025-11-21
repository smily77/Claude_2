/*
 * WT32-SC01 - Einfacher Display Test
 *
 * Sehr einfacher Test um zu prüfen ob das Display grundsätzlich funktioniert.
 * Zeigt nur Farben und Text an.
 */

#include "config.h"

static LGFX tft;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n=== WT32-SC01 Display Test ===");

  // Display initialisieren
  Serial.println("Initialisiere Display...");
  tft.init();

  Serial.print("Display Breite: ");
  Serial.println(tft.width());
  Serial.print("Display Hoehe: ");
  Serial.println(tft.height());

  // Verschiedene Tests
  Serial.println("\nTest 1: Rotes Display");
  tft.fillScreen(0xF800);  // Rot (RGB565)
  delay(2000);

  Serial.println("Test 2: Gruenes Display");
  tft.fillScreen(0x07E0);  // Grün (RGB565)
  delay(2000);

  Serial.println("Test 3: Blaues Display");
  tft.fillScreen(0x001F);  // Blau (RGB565)
  delay(2000);

  Serial.println("Test 4: Weisses Display");
  tft.fillScreen(0xFFFF);  // Weiß
  delay(2000);

  Serial.println("Test 5: Schwarzes Display");
  tft.fillScreen(0x0000);  // Schwarz
  delay(2000);

  // Backlight-Test
  Serial.println("\nTest 6: Backlight Dimmen");
  for (int i = 255; i >= 0; i -= 5) {
    tft.setBrightness(i);
    delay(20);
  }
  delay(500);

  Serial.println("Test 7: Backlight Aufhellen");
  for (int i = 0; i <= 255; i += 5) {
    tft.setBrightness(i);
    delay(20);
  }

  // Text-Test
  Serial.println("\nTest 8: Text anzeigen");
  tft.fillScreen(0x0000);  // Schwarz
  tft.setTextColor(0xFFFF);  // Weiß
  tft.setTextSize(3);
  tft.setCursor(10, 10);
  tft.println("WT32-SC01");
  tft.setCursor(10, 50);
  tft.println("Display Test");

  Serial.println("\n=== Tests abgeschlossen ===");
  Serial.println("Siehst du etwas auf dem Display?");
}

void loop() {
  // Touch-Test
  uint16_t x, y;
  if (tft.getTouch(&x, &y)) {
    Serial.printf("Touch: X=%d, Y=%d\n", x, y);
    tft.fillCircle(x, y, 10, 0xF800);  // Roter Punkt
    delay(100);
  }
  delay(50);
}
