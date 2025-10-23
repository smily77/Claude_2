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

// Funktion zum Zeichnen einer stilisierten Weltkugel
void drawGlobe(int x, int y, int radius) {
  // Schatten für 3D-Effekt
  M5.Display.fillCircle(x + 3, y + 3, radius, TFT_DARKGREY);

  // Hauptkugel (Ozean-Blau)
  M5.Display.fillCircle(x, y, radius, TFT_BLUE);

  // Highlight für glänzenden Effekt
  M5.Display.fillCircle(x - radius/3, y - radius/3, radius/4, TFT_SKYBLUE);

  // Längengrade (vertikal)
  for (int i = -radius; i <= radius; i += radius/2) {
    M5.Display.drawLine(x + i, y - radius, x + i, y + radius, TFT_CYAN);
  }

  // Breitengrade (horizontal) - als Ellipsen für 3D-Effekt
  M5.Display.drawEllipse(x, y - radius/2, radius, radius/4, TFT_CYAN);
  M5.Display.drawEllipse(x, y, radius, radius/2, TFT_CYAN);
  M5.Display.drawEllipse(x, y + radius/2, radius, radius/4, TFT_CYAN);

  // Äquator hervorheben
  M5.Display.drawEllipse(x, y, radius, radius/2, TFT_YELLOW);
  M5.Display.drawEllipse(x, y, radius-1, radius/2-1, TFT_YELLOW);

  // Kontinente (vereinfacht als grüne Formen)
  // Europa/Afrika
  M5.Display.fillCircle(x - radius/4, y - radius/6, radius/5, TFT_GREEN);
  M5.Display.fillCircle(x - radius/6, y + radius/4, radius/6, TFT_GREEN);

  // Amerika
  M5.Display.fillCircle(x + radius/3, y - radius/5, radius/7, TFT_DARKGREEN);
  M5.Display.fillCircle(x + radius/2, y + radius/3, radius/8, TFT_DARKGREEN);

  // Asien
  M5.Display.fillCircle(x - radius/2, y, radius/6, TFT_GREENYELLOW);

  // Rand der Kugel
  M5.Display.drawCircle(x, y, radius, TFT_CYAN);
  M5.Display.drawCircle(x, y, radius-1, TFT_CYAN);
}

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

  // Weltkugel im Vordergrund zeichnen (oben links, überlappt Header und Text)
  drawGlobe(60, 60, 45);
}

void loop() {
  // M5Stack aktualisieren (für Touch und Buttons)
  M5.update();

  // Hier könnte weiterer Code stehen
  delay(100);
}
