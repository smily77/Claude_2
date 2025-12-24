/*
 * M5Stack AtomS3 - Nebelschlussleuchtensymbol
 *
 * Dieses Programm zeigt ein Nebelschlussleuchtensymbol auf dem Display an.
 * Bei jedem Tastendruck ändert sich die Farbe des Symbols.
 *
 * Hardware:
 * - M5Stack AtomS3 mit 128x128 Display
 * - Eingebauter Button
 *
 * Voraussetzungen:
 * - M5AtomS3 Bibliothek installiert
 * - Board: M5Stack-ATOMS3
 */

#include <M5AtomS3.h>

// Farbpalette für das Symbol
uint16_t colors[] = {
  TFT_YELLOW,    // Klassische Nebelleuchten-Farbe
  TFT_ORANGE,
  TFT_RED,
  TFT_GREEN,
  TFT_CYAN,
  TFT_BLUE,
  TFT_MAGENTA,
  TFT_WHITE
};

int currentColorIndex = 0;
const int numColors = sizeof(colors) / sizeof(colors[0]);
bool lastButtonState = false;

void setup() {
  // M5AtomS3 initialisieren
  auto cfg = M5.config();
  AtomS3.begin(cfg);

  // Display initialisieren
  AtomS3.Display.setRotation(0);
  AtomS3.Display.fillScreen(TFT_BLACK);

  // Erstes Symbol zeichnen
  drawFogLightSymbol(colors[currentColorIndex]);
}

void loop() {
  // Button-Status aktualisieren
  AtomS3.update();

  // Button-Press erkennen (mit Entprellung)
  bool currentButtonState = AtomS3.BtnA.isPressed();

  if (currentButtonState && !lastButtonState) {
    // Button wurde gerade gedrückt
    currentColorIndex = (currentColorIndex + 1) % numColors;

    // Display löschen und Symbol neu zeichnen
    AtomS3.Display.fillScreen(TFT_BLACK);
    drawFogLightSymbol(colors[currentColorIndex]);

    delay(200); // Entprellung
  }

  lastButtonState = currentButtonState;
  delay(10);
}

void drawFogLightSymbol(uint16_t color) {
  // Zentrum des Displays
  int centerX = 64;
  int centerY = 64;

  // Linke Seite: Scheinwerfer/Lampe
  // Großer äußerer Halbkreis (links)
  int lampX = centerX - 30;
  int lampY = centerY;
  int lampRadius = 20;

  // Äußerer Halbkreis
  for (int angle = 90; angle <= 270; angle++) {
    float rad = angle * PI / 180.0;
    int x1 = lampX + cos(rad) * lampRadius;
    int y1 = lampY + sin(rad) * lampRadius;
    int x2 = lampX + cos(rad) * (lampRadius - 2);
    int y2 = lampY + sin(rad) * (lampRadius - 2);
    AtomS3.Display.drawLine(lampX, lampY, x1, y1, color);
  }

  // Vertikale Linie rechts vom Halbkreis
  AtomS3.Display.fillRect(lampX, lampY - lampRadius, 3, lampRadius * 2, color);

  // Innerer gefüllter Kreis für die Lampe
  AtomS3.Display.fillCircle(lampX - 10, lampY, 8, color);

  // Rechte Seite: Drei wellenförmige Linien (Nebel)
  int waveStartX = centerX - 5;
  int waveSpacing = 15;

  // Drei wellige Linien zeichnen
  for (int wave = 0; wave < 3; wave++) {
    int x = waveStartX + (wave * waveSpacing);

    // Wellenform mit mehreren Bögen
    drawWaveLine(x, centerY - 15, x + 8, centerY, color, 3);
    drawWaveLine(x + 8, centerY, x, centerY + 15, color, 3);
  }

  // Zusätzliche Verstärkung der Wellenlinien
  for (int wave = 0; wave < 3; wave++) {
    int x = waveStartX + (wave * waveSpacing);
    drawWaveLine(x + 1, centerY - 15, x + 9, centerY, color, 3);
    drawWaveLine(x + 9, centerY, x + 1, centerY + 15, color, 3);
  }
}

void drawWaveLine(int x1, int y1, int x2, int y2, uint16_t color, int thickness) {
  // Zeichnet eine gebogene Linie zwischen zwei Punkten
  int steps = 10;
  float dx = (x2 - x1) / (float)steps;
  float dy = (y2 - y1) / (float)steps;

  for (int i = 0; i < steps; i++) {
    int px1 = x1 + dx * i;
    int py1 = y1 + dy * i;
    int px2 = x1 + dx * (i + 1);
    int py2 = y1 + dy * (i + 1);

    // Mehrere Linien für Dicke
    for (int t = 0; t < thickness; t++) {
      AtomS3.Display.drawLine(px1 - t/2, py1, px2 - t/2, py2, color);
      AtomS3.Display.drawLine(px1 + t/2, py1, px2 + t/2, py2, color);
    }
  }
}
