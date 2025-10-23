/*
 * M5Stack Core2 - Hallo Welt mit Bild
 *
 * Dieses Programm zeigt "Hallo Welt" mit einem echten Bild auf dem Display.
 * Verwendet die M5Unified Bibliothek.
 *
 * Das Bild ist als JPEG-Array in image_data.h eingebettet.
 *
 * Eigene Bilder hinzufügen:
 * 1. Bild herunterladen und auf passende Größe skalieren (z.B. 80x80px)
 * 2. Als JPEG speichern (Qualität 80-90%)
 * 3. Online konvertieren: https://notisrac.github.io/FileToCArray/
 * 4. Array in image_data.h einfügen
 */

#include <M5Unified.h>
#include "image_data.h"

void setup() {
  // M5Stack mit automatischer Konfiguration initialisieren
  auto cfg = M5.config();
  M5.begin(cfg);

  // Display-Hintergrund erstellen
  M5.Display.fillScreen(TFT_BLACK);

  // Dekorativer Header-Bereich
  M5.Display.fillRect(0, 0, 320, 50, TFT_NAVY);
  M5.Display.fillRect(0, 50, 320, 3, TFT_CYAN);

  // Haupttext "Hallo Welt!" mit großem, schönem Font
  M5.Display.setFont(&fonts::FreeSansBold24pt7b);
  M5.Display.setTextColor(TFT_CYAN);
  M5.Display.setTextDatum(middle_center);
  M5.Display.drawString("Hallo Welt!", 160, 120);

  // Untertitel
  M5.Display.setFont(&fonts::FreeSans12pt7b);
  M5.Display.setTextColor(TFT_LIGHTGREY);
  M5.Display.drawString("M5Stack Core2", 160, 170);

  // Info-Text
  M5.Display.setFont(&fonts::FreeSans9pt7b);
  M5.Display.setTextColor(TFT_GREENYELLOW);
  M5.Display.drawString("mit Bild aus dem Internet", 160, 210);

  // Dekorative Linie am unteren Rand
  M5.Display.fillRect(0, 227, 320, 3, TFT_CYAN);
  M5.Display.fillRect(0, 230, 320, 10, TFT_NAVY);

  // Bild anzeigen (oben links, über dem Header)
  // Position: x=20, y=15 (zentriert im Header-Bereich)
  // HINWEIS: Das eingebettete Bild ist nur ein Platzhalter
  // Ersetze es durch dein eigenes Bild in image_data.h
  M5.Display.drawJpg(globe_icon_80x80, globe_icon_80x80_len, 20, 5, 80, 80);

  // Rahmen um das Bild
  M5.Display.drawRect(19, 4, 82, 82, TFT_CYAN);
  M5.Display.drawRect(18, 3, 84, 84, TFT_CYAN);

  // Erfolgs-Meldung auf Serial
  Serial.println("Bild erfolgreich geladen!");
  Serial.printf("Bildgröße: %d Bytes\n", globe_icon_80x80_len);
}

void loop() {
  // M5Stack aktualisieren
  M5.update();

  // Hier könnte weiterer Code stehen
  delay(100);
}
