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

// Globale Variablen für Animation
float rotationAngle = 0.0;
const int globeX = 60;
const int globeY = 55;  // 5 Pixel höher als vorher (war 60)
const int globeRadius = 45;

// Funktion zum Zeichnen einer stilisierten, rotierenden Weltkugel
void drawGlobe(int x, int y, int radius, float angle) {
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

  // Kontinente (vereinfacht als grüne Formen) - rotierend
  // Berechne rotierte Positionen mit Trigonometrie
  float rad = angle * PI / 180.0;

  // Europa/Afrika (Position 0°)
  int europeX1 = x + (int)((-radius/4) * cos(rad) - (-radius/6) * sin(rad));
  int europeY1 = y + (int)((-radius/4) * sin(rad) + (-radius/6) * cos(rad));
  int europeX2 = x + (int)((-radius/6) * cos(rad) - (radius/4) * sin(rad));
  int europeY2 = y + (int)((-radius/6) * sin(rad) + (radius/4) * cos(rad));
  M5.Display.fillCircle(europeX1, europeY1, radius/5, TFT_GREEN);
  M5.Display.fillCircle(europeX2, europeY2, radius/6, TFT_GREEN);

  // Amerika (Position 90°)
  float americaAngle = rad + PI/2;
  int americaX1 = x + (int)((radius/3) * cos(americaAngle));
  int americaY1 = y + (int)((radius/3) * sin(americaAngle) - radius/5);
  int americaX2 = x + (int)((radius/2) * cos(americaAngle));
  int americaY2 = y + (int)((radius/2) * sin(americaAngle) + radius/3);
  M5.Display.fillCircle(americaX1, americaY1, radius/7, TFT_DARKGREEN);
  M5.Display.fillCircle(americaX2, americaY2, radius/8, TFT_DARKGREEN);

  // Asien (Position 180°)
  float asiaAngle = rad + PI;
  int asiaX = x + (int)((radius/2) * cos(asiaAngle));
  int asiaY = y + (int)((radius/2) * sin(asiaAngle));
  M5.Display.fillCircle(asiaX, asiaY, radius/6, TFT_GREENYELLOW);

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

  // Erste Weltkugel im Vordergrund zeichnen
  drawGlobe(globeX, globeY, globeRadius, rotationAngle);
}

void loop() {
  // M5Stack aktualisieren (für Touch und Buttons)
  M5.update();

  // Bereich der Weltkugel löschen (etwas größer für Schatten)
  M5.Display.fillCircle(globeX + 3, globeY + 3, globeRadius + 2, TFT_NAVY);
  M5.Display.fillCircle(globeX, globeY, globeRadius + 2, TFT_NAVY);

  // Obere Cyan-Linie neu zeichnen (falls überzeichnet)
  M5.Display.fillRect(0, 50, 320, 3, TFT_CYAN);

  // Weltkugel mit neuer Rotation zeichnen
  drawGlobe(globeX, globeY, globeRadius, rotationAngle);

  // Rotationswinkel erhöhen (langsame Drehung)
  rotationAngle += 2.0;
  if (rotationAngle >= 360.0) {
    rotationAngle = 0.0;
  }

  // Kurze Pause für flüssige Animation
  delay(50);
}
