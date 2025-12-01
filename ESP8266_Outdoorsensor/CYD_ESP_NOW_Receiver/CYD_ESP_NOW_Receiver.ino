/*
 * CYD ESP-NOW Empfänger für Indoor/Outdoor Sensoren
 *
 * Zeigt Daten von ESP8266 Indoor- und Outdoor-Sensoren auf CYD Display
 *
 * Voraussetzungen:
 * - CYD_Display_Config.h (definiert LGFX Klasse)
 * - LovyanGFX Bibliothek
 * - ESP32 mit Display (240x320 oder 320x480)
 *
 * Features:
 * - Automatische Display-Größen-Erkennung
 * - Zeigt Indoor- und Outdoor-Daten parallel
 * - Schönes, farbenfrohes Design
 * - Batterie-Warnungen
 * - Signal-Stärke (RSSI)
 * - Letzter Empfangszeitpunkt
 */

#include <CYD_Display_Config.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

// ==================== KONFIGURATION ====================

// WiFi Kanal (MUSS mit Sender übereinstimmen!)
#define ESPNOW_CHANNEL 1

// Display Rotation (3 = Landscape)
#define DISPLAY_ROTATION 3

// Update Intervall für "Zeit seit letztem Empfang"
#define TIME_UPDATE_INTERVAL 5000  // 5 Sekunden

// ==================== FARBEN ====================

#define COLOR_BG          0x0008   // Sehr dunkles Blau
#define COLOR_HEADER      0x001F   // Blau
#define COLOR_INDOOR      0x07FF   // Cyan
#define COLOR_OUTDOOR     0xFD20   // Orange
#define COLOR_TEMP        0xF800   // Rot
#define COLOR_HUM         0x07FF   // Cyan
#define COLOR_PRESS       0x07E0   // Grün
#define COLOR_BATTERY_OK  0x07E0   // Grün
#define COLOR_BATTERY_LOW 0xF800   // Rot
#define COLOR_TEXT        0xFFFF   // Weiß
#define COLOR_TEXT_DIM    0x8410   // Grau
#define COLOR_RSSI_GOOD   0x07E0   // Grün
#define COLOR_RSSI_MEDIUM 0xFD20   // Orange
#define COLOR_RSSI_POOR   0xF800   // Rot

// ==================== DATENSTRUKTUR ====================

typedef struct sensor_data_indoor {
  uint32_t timestamp;
  float temperature;
  float pressure;
  float humidity;
  uint8_t am2321_readings;
  uint16_t battery_voltage;
  uint16_t duration;
  uint8_t battery_warning;
  uint8_t sensor_error;
  uint8_t reset_reason;
  uint8_t sensor_type;
} sensor_data_indoor;

typedef struct sensor_data_outdoor {
  uint32_t timestamp;
  float temperature;
  float pressure;
  uint16_t battery_voltage;
  uint16_t duration;
  uint8_t battery_warning;
  uint8_t sensor_error;
  uint8_t reset_reason;
  uint8_t sensor_type;
} sensor_data_outdoor;

// ==================== GLOBALE VARIABLEN ====================

LGFX lcd;  // Display Objekt

sensor_data_indoor indoorData;
sensor_data_outdoor outdoorData;

bool indoorReceived = false;
bool outdoorReceived = false;

unsigned long lastIndoorReceive = 0;
unsigned long lastOutdoorReceive = 0;
unsigned long lastTimeUpdate = 0;

int indoorRSSI = 0;
int outdoorRSSI = 0;

int screenWidth = 320;
int screenHeight = 240;
bool is480p = false;  // true für 320x480, false für 240x320

// ==================== ESP-NOW CALLBACK ====================

void onDataRecv(const esp_now_recv_info* recv_info, const uint8_t *data, int data_len) {
  // Sensor-Typ erkennen
  bool isIndoor = (data_len >= sizeof(sensor_data_indoor));

  if (isIndoor) {
    memcpy(&indoorData, data, min((int)sizeof(indoorData), data_len));
    indoorReceived = true;
    lastIndoorReceive = millis();
    indoorRSSI = recv_info->rx_ctrl->rssi;

    Serial.println("\n=== Indoor Data Received ===");
    Serial.printf("Temp: %.1f°C, Hum: %.1f%%, Press: %.1f mbar\n",
                  indoorData.temperature, indoorData.humidity, indoorData.pressure);
    Serial.printf("Battery: %d mV, RSSI: %d dBm\n", indoorData.battery_voltage, indoorRSSI);

    drawIndoorSection();
  } else {
    memcpy(&outdoorData, data, min((int)sizeof(outdoorData), data_len));
    outdoorReceived = true;
    lastOutdoorReceive = millis();
    outdoorRSSI = recv_info->rx_ctrl->rssi;

    Serial.println("\n=== Outdoor Data Received ===");
    Serial.printf("Temp: %.1f°C, Press: %.1f mbar\n",
                  outdoorData.temperature, outdoorData.pressure);
    Serial.printf("Battery: %d mV, RSSI: %d dBm\n", outdoorData.battery_voltage, outdoorRSSI);

    drawOutdoorSection();
  }
}

// ==================== DISPLAY FUNKTIONEN ====================

// Farbe basierend auf RSSI
uint16_t getRSSIColor(int rssi) {
  if (rssi >= -50) return COLOR_RSSI_GOOD;
  if (rssi >= -70) return COLOR_RSSI_MEDIUM;
  return COLOR_RSSI_POOR;
}

// Zeit formatieren (Sekunden zu String)
String formatTime(unsigned long seconds) {
  if (seconds < 60) {
    return String(seconds) + "s";
  } else if (seconds < 3600) {
    return String(seconds / 60) + "min";
  } else {
    return String(seconds / 3600) + "h " + String((seconds % 3600) / 60) + "min";
  }
}

// Header zeichnen
void drawHeader() {
  // Gradient Header
  int headerHeight = is480p ? 50 : 35;
  for (int y = 0; y < headerHeight; y++) {
    uint8_t blue = 31 - (y * 31 / headerHeight);
    lcd.drawFastHLine(0, y, screenWidth, lcd.color565(0, 0, blue * 8));
  }

  // Titel
  lcd.setFont(is480p ? &fonts::FreeSansBold18pt7b : &fonts::FreeSansBold12pt7b);
  lcd.setTextColor(COLOR_TEXT);
  lcd.setTextDatum(top_center);
  lcd.drawString("Indoor/Outdoor Monitor", screenWidth / 2, is480p ? 10 : 5);
}

// Sensor-Box zeichnen
void drawSensorBox(int x, int y, int w, int h, const char* title, uint16_t color, bool hasData) {
  // Box Rahmen
  lcd.drawRoundRect(x, y, w, h, 8, color);

  if (!hasData) {
    // "Warte auf Daten" anzeigen
    lcd.fillRoundRect(x + 2, y + 2, w - 4, h - 4, 6, COLOR_BG);
  }

  // Titel
  lcd.setFont(&fonts::FreeSansBold9pt7b);
  lcd.setTextColor(color);
  lcd.setTextDatum(top_center);
  lcd.drawString(title, x + w / 2, y + 8);
}

// Indoor Sektion zeichnen
void drawIndoorSection() {
  int boxX = 5;
  int boxY = is480p ? 60 : 45;
  int boxW = screenWidth / 2 - 10;
  int boxH = screenHeight - boxY - 5;

  // Box zeichnen
  drawSensorBox(boxX, boxY, boxW, boxH, "INDOOR", COLOR_INDOOR, indoorReceived);

  if (!indoorReceived) {
    // Warte-Text
    lcd.setFont(&fonts::FreeSans9pt7b);
    lcd.setTextColor(COLOR_TEXT_DIM);
    lcd.setTextDatum(middle_center);
    lcd.drawString("Warte auf", boxX + boxW / 2, boxY + boxH / 2 - 15);
    lcd.drawString("Daten...", boxX + boxW / 2, boxY + boxH / 2 + 10);
    return;
  }

  // Inhalt zeichnen
  int contentY = boxY + 35;
  int lineHeight = is480p ? 35 : 28;
  int centerX = boxX + boxW / 2;

  // Temperatur
  lcd.setFont(is480p ? &fonts::FreeSansBold18pt7b : &fonts::FreeSansBold12pt7b);
  lcd.setTextColor(COLOR_TEMP);
  lcd.setTextDatum(middle_center);
  lcd.drawString(String(indoorData.temperature, 1) + "°C", centerX, contentY);
  contentY += lineHeight;

  // Luftfeuchtigkeit
  lcd.setFont(is480p ? &fonts::FreeSansBold12pt7b : &fonts::FreeSans9pt7b);
  lcd.setTextColor(COLOR_HUM);
  lcd.drawString(String(indoorData.humidity, 0) + "%", centerX, contentY);
  contentY += lineHeight - 5;

  // Luftdruck
  lcd.setTextColor(COLOR_PRESS);
  lcd.drawString(String(indoorData.pressure, 0) + " mbar", centerX, contentY);
  contentY += lineHeight;

  // Batterie
  lcd.setFont(&fonts::FreeSans9pt7b);
  uint16_t battColor = indoorData.battery_warning ? COLOR_BATTERY_LOW : COLOR_BATTERY_OK;
  lcd.setTextColor(battColor);
  lcd.drawString(String(indoorData.battery_voltage) + " mV", centerX, contentY);
  if (indoorData.battery_warning) {
    lcd.setTextColor(COLOR_BATTERY_LOW);
    lcd.drawString("LOW!", centerX, contentY + 18);
  }
  contentY += lineHeight + 5;

  // RSSI
  lcd.setFont(&fonts::FreeSans9pt7b);
  lcd.setTextColor(getRSSIColor(indoorRSSI));
  lcd.drawString("RSSI: " + String(indoorRSSI) + " dBm", centerX, contentY);
  contentY += 22;

  // Letzter Empfang
  unsigned long secondsAgo = (millis() - lastIndoorReceive) / 1000;
  lcd.setFont(&fonts::FreeSans9pt7b);
  lcd.setTextColor(COLOR_TEXT_DIM);
  lcd.drawString(formatTime(secondsAgo), centerX, contentY);
}

// Outdoor Sektion zeichnen
void drawOutdoorSection() {
  int boxX = screenWidth / 2 + 5;
  int boxY = is480p ? 60 : 45;
  int boxW = screenWidth / 2 - 10;
  int boxH = screenHeight - boxY - 5;

  // Box zeichnen
  drawSensorBox(boxX, boxY, boxW, boxH, "OUTDOOR", COLOR_OUTDOOR, outdoorReceived);

  if (!outdoorReceived) {
    // Warte-Text
    lcd.setFont(&fonts::FreeSans9pt7b);
    lcd.setTextColor(COLOR_TEXT_DIM);
    lcd.setTextDatum(middle_center);
    lcd.drawString("Warte auf", boxX + boxW / 2, boxY + boxH / 2 - 15);
    lcd.drawString("Daten...", boxX + boxW / 2, boxY + boxH / 2 + 10);
    return;
  }

  // Inhalt zeichnen
  int contentY = boxY + 35;
  int lineHeight = is480p ? 35 : 28;
  int centerX = boxX + boxW / 2;

  // Temperatur
  lcd.setFont(is480p ? &fonts::FreeSansBold18pt7b : &fonts::FreeSansBold12pt7b);
  lcd.setTextColor(COLOR_TEMP);
  lcd.setTextDatum(middle_center);
  lcd.drawString(String(outdoorData.temperature, 1) + "°C", centerX, contentY);
  contentY += lineHeight;

  // Kein Luftfeuchtigkeit bei Outdoor
  lcd.setFont(is480p ? &fonts::FreeSansBold12pt7b : &fonts::FreeSans9pt7b);
  lcd.setTextColor(COLOR_TEXT_DIM);
  lcd.drawString("(no humidity)", centerX, contentY);
  contentY += lineHeight - 5;

  // Luftdruck
  lcd.setTextColor(COLOR_PRESS);
  lcd.drawString(String(outdoorData.pressure, 0) + " mbar", centerX, contentY);
  contentY += lineHeight;

  // Batterie
  lcd.setFont(&fonts::FreeSans9pt7b);
  uint16_t battColor = outdoorData.battery_warning ? COLOR_BATTERY_LOW : COLOR_BATTERY_OK;
  lcd.setTextColor(battColor);
  lcd.drawString(String(outdoorData.battery_voltage) + " mV", centerX, contentY);
  if (outdoorData.battery_warning) {
    lcd.setTextColor(COLOR_BATTERY_LOW);
    lcd.drawString("LOW!", centerX, contentY + 18);
  }
  contentY += lineHeight + 5;

  // RSSI
  lcd.setFont(&fonts::FreeSans9pt7b);
  lcd.setTextColor(getRSSIColor(outdoorRSSI));
  lcd.drawString("RSSI: " + String(outdoorRSSI) + " dBm", centerX, contentY);
  contentY += 22;

  // Letzter Empfang
  unsigned long secondsAgo = (millis() - lastOutdoorReceive) / 1000;
  lcd.setFont(&fonts::FreeSans9pt7b);
  lcd.setTextColor(COLOR_TEXT_DIM);
  lcd.drawString(formatTime(secondsAgo), centerX, contentY);
}

// Zeit-Updates (ohne komplettes Neuzeichnen)
void updateTimes() {
  // Indoor Zeit Update
  if (indoorReceived) {
    int boxX = 5;
    int boxY = is480p ? 60 : 45;
    int boxW = screenWidth / 2 - 10;
    int boxH = screenHeight - boxY - 5;
    int centerX = boxX + boxW / 2;
    int timeY = boxH - 25;

    // Alten Text löschen
    lcd.fillRect(boxX + 10, boxY + timeY, boxW - 20, 20, COLOR_BG);

    // Neuen Text zeichnen
    unsigned long secondsAgo = (millis() - lastIndoorReceive) / 1000;
    lcd.setFont(&fonts::FreeSans9pt7b);
    lcd.setTextColor(COLOR_TEXT_DIM);
    lcd.setTextDatum(middle_center);
    lcd.drawString(formatTime(secondsAgo), centerX, boxY + timeY + 10);
  }

  // Outdoor Zeit Update
  if (outdoorReceived) {
    int boxX = screenWidth / 2 + 5;
    int boxY = is480p ? 60 : 45;
    int boxW = screenWidth / 2 - 10;
    int boxH = screenHeight - boxY - 5;
    int centerX = boxX + boxW / 2;
    int timeY = boxH - 25;

    // Alten Text löschen
    lcd.fillRect(boxX + 10, boxY + timeY, boxW - 20, 20, COLOR_BG);

    // Neuen Text zeichnen
    unsigned long secondsAgo = (millis() - lastOutdoorReceive) / 1000;
    lcd.setFont(&fonts::FreeSans9pt7b);
    lcd.setTextColor(COLOR_TEXT_DIM);
    lcd.setTextDatum(middle_center);
    lcd.drawString(formatTime(secondsAgo), centerX, boxY + timeY + 10);
  }
}

// ==================== SETUP ====================

void setup() {
  Serial.begin(115200);
  Serial.println("\n\n=== CYD ESP-NOW Receiver ===");

  // Display initialisieren
  lcd.init();
  lcd.setRotation(DISPLAY_ROTATION);
  lcd.setBrightness(255);

  // Display-Größe erkennen
  screenWidth = lcd.width();
  screenHeight = lcd.height();
  is480p = (screenWidth == 480 || screenHeight == 480);

  Serial.printf("Display: %dx%d\n", screenWidth, screenHeight);

  // Hintergrund
  lcd.fillScreen(COLOR_BG);

  // Header zeichnen
  drawHeader();

  // Startbildschirm - beide Boxen
  drawSensorBox(5, is480p ? 60 : 45, screenWidth / 2 - 10,
                screenHeight - (is480p ? 65 : 50), "INDOOR", COLOR_INDOOR, false);
  drawSensorBox(screenWidth / 2 + 5, is480p ? 60 : 45, screenWidth / 2 - 10,
                screenHeight - (is480p ? 65 : 50), "OUTDOOR", COLOR_OUTDOOR, false);

  // Warte-Text
  lcd.setFont(&fonts::FreeSans9pt7b);
  lcd.setTextColor(COLOR_TEXT_DIM);
  lcd.setTextDatum(middle_center);

  // Indoor Warte-Text
  int boxH = screenHeight - (is480p ? 65 : 50);
  int boxY = is480p ? 60 : 45;
  lcd.drawString("Warte auf", screenWidth / 4, boxY + boxH / 2 - 15);
  lcd.drawString("Daten...", screenWidth / 4, boxY + boxH / 2 + 10);

  // Outdoor Warte-Text
  lcd.drawString("Warte auf", screenWidth * 3 / 4, boxY + boxH / 2 - 15);
  lcd.drawString("Daten...", screenWidth * 3 / 4, boxY + boxH / 2 + 10);

  // WiFi im Station Mode starten
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  // WiFi Kanal setzen
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);

  Serial.printf("WiFi Channel: %d\n", ESPNOW_CHANNEL);
  Serial.printf("MAC Address: %s\n", WiFi.macAddress().c_str());

  // ESP-NOW initialisieren
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed!");

    lcd.setFont(&fonts::FreeSansBold12pt7b);
    lcd.setTextColor(COLOR_BATTERY_LOW);
    lcd.setTextDatum(middle_center);
    lcd.drawString("ESP-NOW ERROR!", screenWidth / 2, screenHeight / 2);

    while (1) delay(1000);
  }

  // Receive Callback registrieren
  esp_now_register_recv_cb(onDataRecv);

  Serial.println("Listening for ESP-NOW data...\n");
}

// ==================== LOOP ====================

void loop() {
  // Regelmäßig Zeit-Updates
  if (millis() - lastTimeUpdate >= TIME_UPDATE_INTERVAL) {
    lastTimeUpdate = millis();
    updateTimes();
  }

  delay(100);
}
