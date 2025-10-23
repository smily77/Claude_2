/*
 * M5Stack Core2 - Hallo Welt Programm
 *
 * Dieses Programm zeigt "Hallo Welt" auf dem Display des M5Stack Core2 an.
 * Verwendet die M5Unified Bibliothek für Kompatibilität mit allen M5Stack Geräten.
 *
 * Voraussetzungen:
 * - M5Unified Bibliothek muss in der Arduino IDE installiert sein
 * - Board: M5Stack-Core2
 * - USB-Treiber: CP210x oder CH9102 (je nach Modell)
 */

#include <M5Unified.h>

void setup() {
  // M5Stack mit automatischer Konfiguration initialisieren
  auto cfg = M5.config();
  M5.begin(cfg);

  // Display-Hintergrund auf schwarz setzen
  M5.Display.fillScreen(BLACK);

  // Textgröße setzen (1-7 möglich)
  M5.Display.setTextSize(3);

  // Textfarbe setzen (weiß auf schwarz)
  M5.Display.setTextColor(WHITE);

  // Text zentrieren und anzeigen
  M5.Display.setCursor(80, 110);
  M5.Display.println("Hallo Welt!");

  // Zusätzliche Information anzeigen
  M5.Display.setTextSize(2);
  M5.Display.setCursor(40, 150);
  M5.Display.setTextColor(GREEN);
  M5.Display.println("M5Stack Core2");

  M5.Display.setTextSize(1);
  M5.Display.setCursor(60, 180);
  M5.Display.setTextColor(YELLOW);
  M5.Display.println("mit M5Unified");
}

void loop() {
  // M5Stack aktualisieren (für Touch und Buttons)
  M5.update();

  // Hier könnte weiterer Code stehen
  delay(100);
}
