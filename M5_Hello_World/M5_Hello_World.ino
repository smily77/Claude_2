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

  // Display-Hintergrund mit Farbverlauf erstellen
  M5.Display.fillScreen(TFT_BLACK);

  // Dekorativer Header-Bereich
  M5.Display.fillRect(0, 0, 320, 50, TFT_NAVY);
  M5.Display.fillRect(0, 50, 320, 3, TFT_CYAN);

  // Haupttext "Hallo Welt!" mit großem, schönem Font
  M5.Display.setFont(&fonts::FreeSansBold24pt7b);
  M5.Display.setTextColor(TFT_CYAN);
  M5.Display.setTextDatum(middle_center);  // Zentriert horizontal und vertikal
  M5.Display.drawString("Hallo Welt!", 160, 120);

  // Untertitel mit elegantem Font
  M5.Display.setFont(&fonts::FreeSans12pt7b);
  M5.Display.setTextColor(TFT_LIGHTGREY);
  M5.Display.drawString("M5Stack Core2", 160, 170);

  // Info-Text unten
  M5.Display.setFont(&fonts::FreeSans9pt7b);
  M5.Display.setTextColor(TFT_GREENYELLOW);
  M5.Display.drawString("powered by M5Unified", 160, 210);

  // Dekorative Linie am unteren Rand
  M5.Display.fillRect(0, 227, 320, 3, TFT_CYAN);
  M5.Display.fillRect(0, 230, 320, 10, TFT_NAVY);
}

void loop() {
  // M5Stack aktualisieren (für Touch und Buttons)
  M5.update();

  // Hier könnte weiterer Code stehen
  delay(100);
}
