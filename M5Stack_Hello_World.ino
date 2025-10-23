/*
 * M5Stack Core2 - Hallo Welt Programm
 *
 * Dieses Programm zeigt "Hallo Welt" auf dem Display des M5Stack Core2 an.
 *
 * Voraussetzungen:
 * - M5Core2 Bibliothek muss in der Arduino IDE installiert sein
 * - Board: M5Stack-Core2
 * - USB-Treiber: CP210x oder CH9102 (je nach Modell)
 */

#include <M5Core2.h>

void setup() {
  // M5Stack Core2 initialisieren
  M5.begin();

  // Display-Hintergrund auf schwarz setzen
  M5.Lcd.fillScreen(BLACK);

  // Textgröße setzen (1-7 möglich)
  M5.Lcd.setTextSize(3);

  // Textfarbe setzen (weiß auf schwarz)
  M5.Lcd.setTextColor(WHITE);

  // Text zentrieren und anzeigen
  M5.Lcd.setCursor(80, 110);
  M5.Lcd.println("Hallo Welt!");

  // Zusätzliche Information anzeigen
  M5.Lcd.setTextSize(2);
  M5.Lcd.setCursor(40, 150);
  M5.Lcd.setTextColor(GREEN);
  M5.Lcd.println("M5Stack Core2");
}

void loop() {
  // M5Stack aktualisieren (für Touch und Buttons)
  M5.update();

  // Hier könnte weiterer Code stehen
  delay(100);
}
