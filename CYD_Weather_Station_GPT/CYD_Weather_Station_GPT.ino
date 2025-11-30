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
LGFX_Sprite sprite(&lcd);   // Sprite für double buffering

// 2. Sensor Bibliotheken
#include <Adafruit_AHTX0.h>
#include <Adafruit_BMP280.h>

Adafruit_AHTX0 aht;
Adafruit_BMP280 bmp;

// 3. Konfiguration der Graphen
const int graphWidth  = 240;
const int graphHeight = 50;
const unsigned long updateInterval = 6UL * 60UL * 1000UL; // 6 Minuten
//const unsigned long updateInterval = 1UL * 10UL * 1000UL; // 6 Minuten

float historyTemp[graphWidth];
float historyHum[graphWidth];
float historyPress[graphWidth];

unsigned long lastGraphUpdate = 0;
unsigned long lastSensorRead  = 0;

// ---- Palette-Indices für den 4-Bit-Sprite ----
enum PaletteIndex : uint8_t {
  PAL_BLACK    = 0,
  PAL_WHITE    = 1,
  PAL_DARKGREY = 2,
  PAL_TEMP     = 3,
  PAL_HUM      = 4,
  PAL_PRESS    = 5,
};

// Hilfsfunktion: Array verschieben und neuen Wert anhängen
void pushValue(float* data, float newValue, int size) {
  if (isnan(newValue)) return; 

  for (int i = 0; i < size - 1; i++) {
    data[i] = data[i + 1];
  }
  data[size - 1] = newValue;
}

// Graph auf den SPRITE zeichnen (Farben = Palette-Indizes)
void drawGraph(LGFX_Sprite& g,
               int x, int y,
               float* data,
               int width,
               int height,
               uint8_t colorIndex,       // Palette-Index für die Kurve
               const String& label,
               const String& unit,
               float currentVal) 
{
  // Abschnitt-Hintergrund
  g.fillRect(x, y, 320, height + 30, PAL_DARKGREY); // dunkler Hintergrund aus Palette
  
  // Text
  g.setTextColor(PAL_WHITE, PAL_DARKGREY);
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

  // Min/Max für Autoscaling
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
  
  // Kurve zeichnen
  int prevY = 0;
  for (int i = 0; i < width; i++) {
    if (data[i] == 0 || isnan(data[i])) continue; 
    
    int pixelY = y + height + 25 - (int)((data[i] - minVal) / range * height);
    
    if (i > 0 && data[i - 1] != 0 && !isnan(data[i - 1])) {
      g.drawLine(x + i, prevY, x + i + 1, pixelY, colorIndex); // Palette-Index als Farbe
    }
    prevY = pixelY;
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("--- SYSTEM START (Sprite + Palette) ---");

  // --- Display Initialisierung ---
  pinMode(BACKLIGHT_PIN, OUTPUT);
  digitalWrite(BACKLIGHT_PIN, HIGH);
  lcd.init();
  lcd.setRotation(1);
  lcd.setBrightness(255); 
  lcd.fillScreen(TFT_BLACK); 

  // --- Sprite mit 4-Bit-Farbtiefe ---
  sprite.setColorDepth(4);          // 4 Bit = 16 Farben
  if (!sprite.createSprite(320, 240)) {
    Serial.println("FEHLER: Sprite konnte nicht erstellt werden!");
  } else {
    // Palette definieren (16 Einträge möglich, wir nutzen einige davon)
    sprite.setPaletteColor(PAL_BLACK,    TFT_BLACK);
    sprite.setPaletteColor(PAL_WHITE,    TFT_WHITE);
    sprite.setPaletteColor(PAL_DARKGREY, 0x10A2);      // dein dunkelgrauer Hintergrund
    sprite.setPaletteColor(PAL_TEMP,     TFT_ORANGE);  // Temperatur-Kurve
    sprite.setPaletteColor(PAL_HUM,      TFT_CYAN);    // Feuchte-Kurve
    sprite.setPaletteColor(PAL_PRESS,    TFT_GREEN);   // Druck-Kurve

    sprite.fillScreen(PAL_BLACK);
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
  float currentTemp = aht_read_ok ? temp_aht.temperature       : NAN;
  float currentHum  = aht_read_ok ? humidity.relative_humidity : NAN;
  
  float pressure_pa  = bmp.readPressure(); 
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
    // komplettes Frame in Sprite aufbauen
    sprite.fillScreen(PAL_BLACK);
    
    drawGraph(sprite, 0,   0, historyTemp,  graphWidth, 40, PAL_TEMP,  "Temp",   "C",   currentTemp);
    drawGraph(sprite, 0,  80, historyHum,   graphWidth, 40, PAL_HUM,   "Feuchte","%",   currentHum);
    drawGraph(sprite, 0, 160, historyPress, graphWidth, 40, PAL_PRESS, "Druck",  "hP", currentPress);

    // und dann in einem Rutsch auf das Display
    sprite.pushSprite(0, 0);

    lastSensorRead = millis();
  }
}
