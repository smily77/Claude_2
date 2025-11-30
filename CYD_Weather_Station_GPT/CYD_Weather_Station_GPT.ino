#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>

// 1. Display Konfiguration einbinden
#include <CYD_Display_Config.h>

// Fallback und Backlight Pin
#ifndef extSDA
#define extSDA 22
#endif
#ifndef extSCL
#define extSCL 27
#endif
#define BACKLIGHT_PIN 21 

// LGFX Objekt erstellen
LGFX lcd;
LGFX_Sprite sprite(&lcd);   // <-- wieder aktiv

// 2. Sensor Bibliotheken
#include <Adafruit_AHTX0.h>
#include <Adafruit_BMP280.h>

Adafruit_AHTX0 aht;
Adafruit_BMP280 bmp;

// 3. Konfiguration der Graphen
const int graphWidth  = 240;
const int graphHeight = 50;
const unsigned long updateInterval = 6UL * 60UL * 1000UL; // 6 Minuten

float historyTemp[graphWidth];
float historyHum[graphWidth];
float historyPress[graphWidth];

unsigned long lastGraphUpdate = 0;
unsigned long lastSensorRead  = 0;


// Hilfsfunktion: Array verschieben und neuen Wert anhängen
void pushValue(float* data, float newValue, int size) {
  if (isnan(newValue)) return; 

  for (int i = 0; i < size - 1; i++) {
    data[i] = data[i + 1];
  }
  data[size - 1] = newValue;
}

// Template-Hilfsfunktion: Graph zeichnen
// funktioniert mit jedem Objekt, das die LovyanGFX-API hat (lcd, sprite, …)
template<typename G>
void drawGraph(G& g,
               int x, int y,
               float* data,
               int width,
               int height,
               uint16_t color,
               const String& label,
               const String& unit,
               float currentVal) 
{
  // Rahmen und Hintergrund für den Abschnitt
  g.fillRect(x, y, 320, height + 30, 0x10A2); // Dunkelgrau Hintergrund
  
  // Text Ausgabe (Großer Font)
  g.setTextColor(TFT_WHITE, 0x10A2);
  g.setFont(&fonts::FreeSansBold12pt7b);
  g.setCursor(x + 5, y + 5);
  g.print(label); 
  
  g.setFont(&fonts::FreeSansBold18pt7b);
  g.setCursor(x + 180, y + 5);
  
  if (isnan(currentVal)) {
    g.print("--.-");
  } else {
    g.printf("%.1f %s", currentVal, unit.c_str());
  }

  // Min/Max für Autoscaling finden
  float minVal = 10000;
  float maxVal = -10000;
  bool hasData = false;

  for (int i = 0; i < width; i++) {
    if (data[i] != 0 && !isnan(data[i])) {
      if (data[i] < minVal) minVal = data[i];
      if (data[i] > maxVal) maxVal = data[i];
      hasData = true;
    }
  }

  if (!hasData) return;

  float range = maxVal - minVal;
  if (range == 0) range = 1.0f;
  
  // Graph zeichnen
  int prevY = 0;
  for (int i = 0; i < width; i++) {
    if (data[i] == 0 || isnan(data[i])) continue; 
    
    int pixelY = y + height + 25 - (int)((data[i] - minVal) / range * height);
    
    if (i > 0 && data[i - 1] != 0 && !isnan(data[i - 1])) {
      g.drawLine(x + i, prevY, x + i + 1, pixelY, color);
    }
    prevY = pixelY;
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("--- SYSTEM START (Sprite-Version) ---");

  // --- Display Initialisierung ---
  pinMode(BACKLIGHT_PIN, OUTPUT);
  digitalWrite(BACKLIGHT_PIN, HIGH);
  lcd.init();
  lcd.setRotation(1);
  lcd.setBrightness(255); 
  lcd.fillScreen(TFT_BLACK); 

  // Sprite anlegen (4-Bit Farbtiefe für CDY mit wenig RAM)
  sprite.setColorDepth(4);          // 4 Bit = 16 Farben, ~38 kB für 320x240
  if (!sprite.createSprite(320, 240)) {
    Serial.println("FEHLER: Sprite konnte nicht erstellt werden!");
  } else {
    sprite.fillScreen(TFT_BLACK);
  }
  
  // --- Sensoren Initialisierung ---
  Serial.println("Starte I2C...");
  Wire.begin(extSDA, extSCL);
  delay(50); 

  if (!aht.begin()) {
    Serial.println("FEHLER: AHT20 nicht gefunden!");
  } else {
    Serial.println("AHT20 OK.");
  }

  if (!bmp.begin(0x77)) {
    Serial.println("FEHLER: BMP280 nicht gefunden! (Adresse 0x77)");
  } else {
    Serial.println("BMP280 OK.");
  }

  for (int i = 0; i < graphWidth; i++) {
    historyTemp[i]  = 0;
    historyHum[i]   = 0;
    historyPress[i] = 0;
  }
  Serial.println("Setup abgeschlossen. Starte Loop.");
}

void loop() {
  sensors_event_t humidity, temp_aht;
  
  bool aht_read_ok = aht.getEvent(&humidity, &temp_aht);
  float currentTemp = aht_read_ok ? temp_aht.temperature          : NAN;
  float currentHum  = aht_read_ok ? humidity.relative_humidity    : NAN;
  
  float pressure_pa = bmp.readPressure(); 
  float currentPress = pressure_pa > 500.0F ? pressure_pa / 100.0F : NAN; 

  // --- Daten Historie Update (alle 6 Minuten) ---
  if (millis() - lastGraphUpdate > updateInterval) {
    pushValue(historyTemp,  currentTemp,  graphWidth);
    pushValue(historyHum,   currentHum,   graphWidth);
    pushValue(historyPress, currentPress, graphWidth);
    lastGraphUpdate = millis();
  }

  // --- Zeichnen (ca. jede Sekunde) ---
  if (millis() - lastSensorRead > 1000) { 
    // NICHT mehr direkt das LCD löschen -> kein Flackern
    // Stattdessen: komplettes Frame in den Sprite zeichnen
    sprite.fillScreen(TFT_BLACK);
    
    drawGraph(sprite, 0,   0,  historyTemp,  graphWidth, 40, TFT_ORANGE, "Temp",   "C",   currentTemp);
    drawGraph(sprite, 0,  80,  historyHum,   graphWidth, 40, TFT_CYAN,   "Feuchte","%",   currentHum);
    drawGraph(sprite, 0, 160,  historyPress, graphWidth, 40, TFT_GREEN,  "Druck",  "hPa", currentPress);

    // Am Ende EIN push auf das Display
    sprite.pushSprite(0, 0);

    lastSensorRead = millis();
  }
}
