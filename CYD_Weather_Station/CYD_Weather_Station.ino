/*
 * CYD Wetterstation
 *
 * Zeigt Temperatur, Luftfeuchtigkeit und Luftdruck mit 24-Stunden-Graphen
 *
 * Voraussetzungen:
 * - CYD_Display_Config.h muss vorhanden sein (definiert LGFX Klasse und optional extSDA/extSCL)
 * - BMP280 Sensor (I2C 0x77) - Temperatur & Luftdruck
 * - AHT20 Sensor (I2C 0x38) - Temperatur & Luftfeuchtigkeit
 * - I2C Pins: extSDA und extSCL (Standard: GPIO 22 und 27)
 *
 * Bibliotheken (über Arduino Library Manager):
 * - LovyanGFX
 * - Adafruit BMP280 Library
 * - Adafruit AHTX0
 */

#include <CYD_Display_Config.h>
#include <Wire.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_AHTX0.h>

// Fallback für I2C Pins, falls nicht im Header definiert
#ifndef extSDA
#define extSDA 22
#endif
#ifndef extSCL
#define extSCL 27
#endif

// Display Objekt erstellen
LGFX lcd;

// Sensor Objekte
Adafruit_BMP280 bmp;
Adafruit_AHTX0 aht;

// Farben definieren
#define COLOR_BG       0x0008      // Sehr dunkles Blau
#define COLOR_HEADER   0x001F      // Blau
#define COLOR_TEMP     0xFD20      // Orange
#define COLOR_HUM      0x07FF      // Cyan
#define COLOR_PRESS    0x07E0      // Grün
#define COLOR_GRAPH_BG 0x18E3      // Dunkelgrau
#define COLOR_GRID     0x39E7      // Hellgrau
#define COLOR_TEXT     0xFFFF      // Weiß

// Datenspeicher für 24 Stunden (96 Werte = alle 15 Minuten)
#define DATA_POINTS 96
float tempHistory[DATA_POINTS];
float humHistory[DATA_POINTS];
float pressHistory[DATA_POINTS];
int currentDataPoint = 0;
bool dataFilled = false;

// Timing
unsigned long lastSensorRead = 0;
unsigned long lastDataStore = 0;
const unsigned long SENSOR_INTERVAL = 10000;  // 10 Sekunden
const unsigned long STORE_INTERVAL = 10000;   // 10 Sekunden (TEST - normal: 900000 = 15 Min)

// Aktuelle Werte
float currentTemp = 0;
float currentHum = 0;
float currentPress = 0;

// Status
bool bmpOk = false;
bool ahtOk = false;

void setup() {
  Serial.begin(115200);
  Serial.println("CYD Wetterstation startet...");

  // Display initialisieren
  lcd.init();
  lcd.setRotation(1);  // Landscape (320x240)
  lcd.setBrightness(255);
  lcd.fillScreen(COLOR_BG);

  // Startbildschirm
  lcd.setFont(&fonts::FreeSansBold18pt7b);
  lcd.setTextColor(COLOR_HEADER);
  lcd.setTextDatum(middle_center);
  lcd.drawString("Wetterstation", 160, 60);

  lcd.setFont(&fonts::FreeSans12pt7b);
  lcd.setTextColor(COLOR_TEXT);
  lcd.drawString("Initialisiere...", 160, 120);

  // I2C initialisieren
  Wire.begin(extSDA, extSCL);
  Wire.setClock(400000);  // 400kHz
  delay(100);

  // BMP280 initialisieren
  lcd.drawString("BMP280...", 160, 160);
  if (bmp.begin(0x77, 0x58)) {
    bmpOk = true;
    // Konfiguration für Wetterstation
    bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                    Adafruit_BMP280::SAMPLING_X2,   // Temperatur oversampling
                    Adafruit_BMP280::SAMPLING_X16,  // Druck oversampling
                    Adafruit_BMP280::FILTER_X16,    // Filter
                    Adafruit_BMP280::STANDBY_MS_500);
    Serial.println("BMP280 OK");
  } else {
    Serial.println("BMP280 Fehler!");
  }

  // AHT20 initialisieren
  lcd.drawString("AHT20...", 160, 190);
  if (aht.begin()) {
    ahtOk = true;
    Serial.println("AHT20 OK");
  } else {
    Serial.println("AHT20 Fehler!");
  }

  delay(1000);

  // Historische Daten initialisieren
  for (int i = 0; i < DATA_POINTS; i++) {
    tempHistory[i] = 0;
    humHistory[i] = 0;
    pressHistory[i] = 0;
  }

  // Erste Messung
  readSensors();

  // Ersten Datenpunkt speichern
  storeDataPoint();

  // Hauptbildschirm zeichnen
  drawMainScreen();

  Serial.println("Wetterstation bereit!");
}

void loop() {
  unsigned long now = millis();

  // Sensoren regelmäßig auslesen
  if (now - lastSensorRead >= SENSOR_INTERVAL) {
    lastSensorRead = now;
    readSensors();
    updateDisplay();
  }

  // Datenpunkt für Graphen speichern
  if (now - lastDataStore >= STORE_INTERVAL) {
    lastDataStore = now;
    storeDataPoint();
    drawGraphs();  // Graphen neu zeichnen
  }

  delay(100);
}

void readSensors() {
  // BMP280 auslesen
  if (bmpOk) {
    currentTemp = bmp.readTemperature();
    currentPress = bmp.readPressure() / 100.0;  // hPa

    Serial.print("BMP280 - Temp: ");
    Serial.print(currentTemp);
    Serial.print("°C, Druck: ");
    Serial.print(currentPress);
    Serial.println(" hPa");
  }

  // AHT20 auslesen
  if (ahtOk) {
    sensors_event_t humidity, temp;
    aht.getEvent(&humidity, &temp);

    // Durchschnitt aus beiden Sensoren für genauere Temperatur
    if (bmpOk) {
      currentTemp = (currentTemp + temp.temperature) / 2.0;
    } else {
      currentTemp = temp.temperature;
    }

    currentHum = humidity.relative_humidity;

    Serial.print("AHT20 - Temp: ");
    Serial.print(temp.temperature);
    Serial.print("°C, Hum: ");
    Serial.print(currentHum);
    Serial.println("%");
  }
}

void storeDataPoint() {
  tempHistory[currentDataPoint] = currentTemp;
  humHistory[currentDataPoint] = currentHum;
  pressHistory[currentDataPoint] = currentPress;

  currentDataPoint++;
  if (currentDataPoint >= DATA_POINTS) {
    currentDataPoint = 0;
    dataFilled = true;
  }

  Serial.print("Datenpunkt gespeichert: ");
  Serial.println(currentDataPoint);
}

void drawMainScreen() {
  lcd.fillScreen(COLOR_BG);

  // Header mit Gradient
  for (int y = 0; y < 30; y++) {
    uint8_t blue = 31 - (y * 31 / 30);
    lcd.drawFastHLine(0, y, 320, lcd.color565(0, 0, blue * 8));
  }

  // Titel
  lcd.setFont(&fonts::FreeSansBold12pt7b);
  lcd.setTextColor(COLOR_TEXT);
  lcd.setTextDatum(top_center);
  lcd.drawString("Wetterstation 24h", 160, 5);

  // Bereiche für Werte (obere Hälfte)
  drawValueBoxes();

  // Graphen (untere Hälfte)
  drawGraphs();
}

void drawValueBoxes() {
  // Drei Boxen für aktuelle Werte
  int boxWidth = 100;
  int boxHeight = 70;
  int boxY = 35;
  int spacing = 10;

  // Temperatur Box
  lcd.drawRoundRect(5, boxY, boxWidth, boxHeight, 5, COLOR_TEMP);

  // Luftfeuchtigkeit Box
  lcd.drawRoundRect(110, boxY, boxWidth, boxHeight, 5, COLOR_HUM);

  // Luftdruck Box
  lcd.drawRoundRect(215, boxY, boxWidth, boxHeight, 5, COLOR_PRESS);

  // Icons/Labels
  lcd.setFont(&fonts::FreeSans9pt7b);

  lcd.setTextColor(COLOR_TEMP);
  lcd.setTextDatum(top_center);
  lcd.drawString("Temp", 55, boxY + 5);

  lcd.setTextColor(COLOR_HUM);
  lcd.drawString("Feuchte", 160, boxY + 5);

  lcd.setTextColor(COLOR_PRESS);
  lcd.drawString("Druck", 265, boxY + 5);

  // Werte anzeigen
  updateDisplay();
}

void updateDisplay() {
  int boxY = 35;
  int boxHeight = 70;

  // Alte Werte übermalen
  lcd.fillRect(10, boxY + 28, 90, 38, COLOR_BG);
  lcd.fillRect(115, boxY + 28, 90, 38, COLOR_BG);
  lcd.fillRect(220, boxY + 28, 90, 38, COLOR_BG);

  // Temperatur
  lcd.setFont(&fonts::FreeSansBold18pt7b);
  lcd.setTextColor(COLOR_TEMP);
  lcd.setTextDatum(top_center);
  String tempStr = String(currentTemp, 1);
  lcd.drawString(tempStr, 55, boxY + 30);

  lcd.setFont(&fonts::Font2);  // Kleinere Schrift für Einheit
  lcd.drawString("C", 55, boxY + 58);

  // Luftfeuchtigkeit
  lcd.setFont(&fonts::FreeSansBold18pt7b);
  lcd.setTextColor(COLOR_HUM);
  String humStr = String((int)currentHum);
  lcd.drawString(humStr, 160, boxY + 30);

  lcd.setFont(&fonts::Font2);  // Kleinere Schrift für Einheit
  lcd.drawString("%", 160, boxY + 58);

  // Luftdruck
  lcd.setFont(&fonts::FreeSansBold18pt7b);
  lcd.setTextColor(COLOR_PRESS);
  String pressStr = String((int)currentPress);
  lcd.drawString(pressStr, 265, boxY + 30);

  lcd.setFont(&fonts::Font2);  // Kleinere Schrift für Einheit
  lcd.drawString("hPa", 265, boxY + 58);
}

void drawGraphs() {
  // Drei kleine Graphen untereinander
  int graphX = 10;
  int graphWidth = 300;
  int graphHeight = 30;
  int graphY1 = 115;  // Temperatur
  int graphY2 = 155;  // Luftfeuchtigkeit
  int graphY3 = 195;  // Luftdruck

  // Temperatur Graph
  drawGraph(graphX, graphY1, graphWidth, graphHeight,
            tempHistory, "T", COLOR_TEMP, -10, 40);

  // Luftfeuchtigkeit Graph
  drawGraph(graphX, graphY2, graphWidth, graphHeight,
            humHistory, "H", COLOR_HUM, 0, 100);

  // Luftdruck Graph
  drawGraph(graphX, graphY3, graphWidth, graphHeight,
            pressHistory, "P", COLOR_PRESS, 950, 1050);
}

void drawGraph(int x, int y, int w, int h, float* data,
               const char* label, uint16_t color, float minVal, float maxVal) {
  // Hintergrund
  lcd.fillRoundRect(x, y, w, h, 3, COLOR_GRAPH_BG);
  lcd.drawRoundRect(x, y, w, h, 3, color);

  // Label
  lcd.setFont(&fonts::FreeSans9pt7b);
  lcd.setTextColor(color);
  lcd.setTextDatum(middle_left);
  lcd.drawString(label, x + 3, y + h/2);

  // Datenpunkte zeichnen
  int dataCount = dataFilled ? DATA_POINTS : currentDataPoint;
  if (dataCount < 2) return;

  // Bereich für Graph (ohne Label)
  int graphStart = x + 20;
  int graphWidth = w - 25;

  // Min/Max Werte finden für Auto-Scaling
  float actualMin = minVal;
  float actualMax = maxVal;
  bool autoScale = false;

  if (autoScale) {
    actualMin = data[0];
    actualMax = data[0];
    for (int i = 1; i < dataCount; i++) {
      if (data[i] < actualMin) actualMin = data[i];
      if (data[i] > actualMax) actualMax = data[i];
    }
    // Etwas Spielraum
    float range = actualMax - actualMin;
    actualMin -= range * 0.1;
    actualMax += range * 0.1;
  }

  // Graph zeichnen
  for (int i = 0; i < dataCount - 1; i++) {
    int idx1 = dataFilled ? (currentDataPoint + i) % DATA_POINTS : i;
    int idx2 = dataFilled ? (currentDataPoint + i + 1) % DATA_POINTS : i + 1;

    float val1 = data[idx1];
    float val2 = data[idx2];

    // In Pixel-Koordinaten umrechnen
    int x1 = graphStart + (i * graphWidth / (dataCount - 1));
    int x2 = graphStart + ((i + 1) * graphWidth / (dataCount - 1));

    int y1 = y + h - 2 - ((val1 - actualMin) / (actualMax - actualMin) * (h - 4));
    int y2 = y + h - 2 - ((val2 - actualMin) / (actualMax - actualMin) * (h - 4));

    // Linie zeichnen
    lcd.drawLine(x1, y1, x2, y2, color);

    // Optionale Punkte
    if (dataCount < 50) {
      lcd.fillCircle(x1, y1, 1, color);
    }
  }

  // Letzter Punkt
  if (dataCount > 0) {
    int idx = dataFilled ? (currentDataPoint - 1 + DATA_POINTS) % DATA_POINTS : dataCount - 1;
    float val = data[idx];
    int px = graphStart + graphWidth;
    int py = y + h - 2 - ((val - actualMin) / (actualMax - actualMin) * (h - 4));
    lcd.fillCircle(px, py, 2, color);
  }
}
