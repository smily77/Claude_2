/*
 * CYD_I2C_Master.ino
 * 
 * CYD Display als I2C Master
 * Liest Sensordaten von ESP32-C3 Bridge und zeigt sie an
 * Kann gleichzeitig WiFi für NTP nutzen
 * 
 * Hardware:
 * - CYD gmäss CYD_Display_Config.h
 * - I2C: extSDA Pin 22, extSDA Pin 27 soweit in der CYD_Display_Config.h nicht anderst definiert
 * - Pull-up Widerstände 4.7k an SDA/SCL
 */

#include <CYD_Display_Config.h>
#include <Credentials.h>
#include <WiFi.h>
#include <time.h>
#include <SD.h>
#include <SPI.h>
#include <WebServer.h>
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>
#include "I2CSensorBridge.h"

// ==================== KONFIGURATION ====================

// I2C Konfiguration
#ifndef extSDA
  #define extSDA 22
#endif

#ifndef extSCL
  #define extSCL 27
#endif

#define I2C_FREQUENCY 100000           // 100 kHz Standard

// Bridge I2C Adressen
#define BRIDGE_ADDRESS_1 0x20          // Bridge Adresse (ESP32-C3)
// #define BRIDGE_ADDRESS_2 0x21       // Zweite Bridge (falls vorhanden)

// SD-Karte Konfiguration (CYD Standard-Pins)
#define SD_CS    5
#define SD_MOSI  23
#define SD_MISO  19
#define SD_SCK   18

// Eigener SPI-Bus für SD-Karte (VSPI)
SPIClass sdSPI(VSPI);

// WiFi Konfiguration (optional für NTP)

#define NTP_SERVER "pool.ntp.org"
#define GMT_OFFSET_SEC 3600           // GMT+1 für Deutschland/Schweiz
#define DAYLIGHT_OFFSET_SEC 3600      // Sommerzeit

// Display
#define DISPLAY_ROTATION 3             // Landscape

// Update-Intervalle (ms)
#define I2C_POLL_INTERVAL 1000        // I2C alle 1 Sekunde abfragen
#define DISPLAY_UPDATE_INTERVAL 5000  // Display-Zeit alle 5 Sekunden
#define WIFI_RETRY_INTERVAL 30000     // WiFi-Reconnect alle 30 Sekunden
#define SD_LOG_INTERVAL 900000        // SD-Log alle 15 Minuten (900000 ms)

// ==================== INFLUXDB KONFIGURATION ====================
// WICHTIG: Setze diese Werte in deiner Credentials.h oder hier direkt

// InfluxDB aktivieren/deaktivieren (auskommentieren zum Deaktivieren)
// #define ENABLE_INFLUXDB  // Auskommentiert: Nur CSV-Logging (für QNAP TS-210 ohne Container Station)

#ifdef ENABLE_INFLUXDB
  // InfluxDB v2 Server URL (z.B. http://192.168.1.100:8086)
  #ifndef INFLUXDB_URL
    #define INFLUXDB_URL "http://192.168.1.100:8086"
  #endif

  // InfluxDB v2 Token (wird in InfluxDB UI erstellt)
  #ifndef INFLUXDB_TOKEN
    #define INFLUXDB_TOKEN "dein-token-hier-eintragen"
  #endif

  // Organisation Name
  #ifndef INFLUXDB_ORG
    #define INFLUXDB_ORG "home"
  #endif

  // Bucket Name (wie eine Datenbank)
  #ifndef INFLUXDB_BUCKET
    #define INFLUXDB_BUCKET "sensors"
  #endif

  // Timezone für InfluxDB
  #define TZ_INFO "CET-1CEST,M3.5.0,M10.5.0/3"
#endif

// ==================== DATENSTRUKTUREN ====================

// Müssen EXAKT mit Bridge übereinstimmen!
struct IndoorData {
    float temperature;      // °C
    float humidity;        // %
    float pressure;        // mbar
    uint16_t battery_mv;   // mV
    uint32_t timestamp;    // ms
    int8_t rssi;          // dBm
    bool battery_warning;
    uint16_t sleep_time_sec; // Sleep-Periode in Sekunden
} __attribute__((packed));

struct OutdoorData {
    float temperature;      // °C
    float pressure;        // mbar
    uint16_t battery_mv;   // mV
    uint32_t timestamp;    // ms
    int8_t rssi;          // dBm
    bool battery_warning;
    uint16_t sleep_time_sec; // Sleep-Periode in Sekunden
} __attribute__((packed));

struct SystemStatus {
    unsigned long indoor_last_seen;   // Millisekunden
    unsigned long outdoor_last_seen;  // Millisekunden
    uint16_t esp_now_packets;
    uint8_t wifi_channel;
} __attribute__((packed));

// ==================== DISPLAY FARBEN ====================

#define COLOR_BG          0x0002   // Dunkles Blau
#define COLOR_HEADER      0x001F   // Blau
#define COLOR_INDOOR      0x07FF   // Cyan
#define COLOR_OUTDOOR     0xFD20   // Orange
#define COLOR_TEMP        0xFFFF   // Weiß
#define COLOR_HUM         0x07FF   // Cyan
#define COLOR_PRESS       0x07E0   // Grün
#define COLOR_BATTERY_OK  0x0500   // Grün
#define COLOR_BATTERY_LOW 0xF800   // Rot
#define COLOR_TEXT        0xFFFF   // Weiß
#define COLOR_TEXT_DIM    0x8410   // Grau
#define COLOR_RSSI_GOOD   0x0500   // Grün
#define COLOR_RSSI_MEDIUM 0x6800   // Orange
#define COLOR_RSSI_POOR   0x6000   // Dunkelrot

// ==================== MIN/MAX TRACKING ====================

// Struktur für Min/Max-Werte
struct MinMaxData {
    float tempMin;
    float tempMax;
    float humMin;           // nur Indoor
    float humMax;           // nur Indoor
    float pressMin;
    float pressMax;
    uint16_t batteryMin;
    uint16_t batteryMax;
    unsigned long lastReset;  // Zeitstempel des letzten Resets
};

// ==================== GLOBALE VARIABLEN ====================

// Display
LGFX lcd;
LGFX_Sprite indoorSprite(&lcd);
LGFX_Sprite outdoorSprite(&lcd);
int screenWidth = 320;
int screenHeight = 240;
bool is480p = false;

// Display Mode
uint8_t displayMode = 0;       // 0 = Normal, 1 = Outdoor Graph, 2 = Battery Graph, 3 = Min/Max
unsigned long lastTouchTime = 0;
unsigned long lastModeChange = 0;
const unsigned long TOUCH_DEBOUNCE = 300;  // 300ms Debounce
const unsigned long AUTO_RETURN_TIME = 30000;  // 30s Auto-Return zu Schirm 1

// Graph Daten (240 Messwerte)
#define GRAPH_DATA_POINTS 240
struct GraphData {
    // Outdoor
    float outdoorTempValues[GRAPH_DATA_POINTS];
    float outdoorPressValues[GRAPH_DATA_POINTS];
    uint16_t outdoorBatteryValues[GRAPH_DATA_POINTS];

    // Indoor
    uint16_t indoorBatteryValues[GRAPH_DATA_POINTS];

    bool midnightMarker[GRAPH_DATA_POINTS];  // True wenn Wert nahe Mitternacht
    uint16_t dataCount;  // Anzahl gültiger Datenpunkte
    bool outdoorLoaded;
    bool indoorLoaded;
} graphData;

// I2C Bridge
I2CSensorBridge i2cBridge;

// Daten
IndoorData indoorData;
OutdoorData outdoorData;
SystemStatus systemStatus;

// Min/Max Tracking
MinMaxData indoorMinMax;
MinMaxData outdoorMinMax;

// Status
bool indoorReceived = false;
bool outdoorReceived = false;
unsigned long lastIndoorUpdate = 0;
unsigned long lastOutdoorUpdate = 0;

// Timing
unsigned long lastI2CPoll = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long lastWiFiRetry = 0;

// WiFi & Zeit
bool wifiConnected = false;
bool timeConfigured = false;

// SD-Karte
bool sdCardAvailable = false;
unsigned long lastSDLog = 0;
String currentDateString = "";

// Webserver
WebServer server(80);

// InfluxDB Client
#ifdef ENABLE_INFLUXDB
  InfluxDBClient influxClient(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN);
  bool influxDBConnected = false;
  unsigned long lastInfluxRetry = 0;
  #define INFLUX_RETRY_INTERVAL 60000  // Retry alle 60 Sekunden
#endif

// ==================== DISPLAY FUNKTIONEN ====================

uint16_t getRSSIColor(int rssi) {
    if (rssi >= -50) return COLOR_RSSI_GOOD;
    if (rssi >= -70) return COLOR_RSSI_MEDIUM;
    return COLOR_RSSI_POOR;
}

String formatTime(unsigned long seconds) {
    if (seconds > 999999) return "Never";
    if (seconds < 60) return String(seconds) + "s";
    if (seconds < 3600) return String(seconds / 60) + "min";
    return String(seconds / 3600) + "h " + String((seconds % 3600) / 60) + "min";
}

void drawHeader() {
    // Gradient Header
    int headerHeight = is480p ? 50 : 35;
    for (int y = 0; y < headerHeight; y++) {
        uint8_t blue = 31 - (y * 31 / headerHeight);
        lcd.drawFastHLine(0, y, screenWidth, lcd.color565(0, 0, blue * 8));
    }

    // Monatsnamen für deutsches Datum
    const char* monate[] = {"Jan.", "Feb.", "Mrz.", "Apr.", "Mai", "Jun.",
                            "Jul.", "Aug.", "Sep.", "Okt.", "Nov.", "Dez."};

    // Zeit & Datum auf einer Zeile, zentriert
    if (wifiConnected && timeConfigured) {
        struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
            lcd.setFont(is480p ? &fonts::FreeSansBold18pt7b : &fonts::FreeSansBold12pt7b);
            lcd.setTextColor(COLOR_TEXT);
            lcd.setTextDatum(top_center);

            char timeStr[6];
            strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo);

            char timeDateStr[32];
            snprintf(timeDateStr, sizeof(timeDateStr), "%s  %d. %s", timeStr, timeinfo.tm_mday, monate[timeinfo.tm_mon]);

            lcd.drawString(timeDateStr, screenWidth / 2, is480p ? 10 : 7);
        }
    }

    // I2C Bridge Status (links oben)
    lcd.setFont(&fonts::Font2);
    lcd.setTextDatum(top_left);
    if (systemStatus.esp_now_packets > 0) {
        lcd.setTextColor(COLOR_RSSI_GOOD);
        lcd.drawString("Bridge OK", 5, 0);
    } else {
        lcd.setTextColor(COLOR_RSSI_MEDIUM);
        lcd.drawString("Waiting...", 5, 0);
    }

    // SD-Karten Status (links unten, im blauen Bereich)
    if (sdCardAvailable) {
        lcd.setTextColor(COLOR_RSSI_GOOD);
        lcd.drawString("SD", 5, 20);
    }

    // WiFi Status (rechts oben)
    lcd.setTextDatum(top_right);
    if (wifiConnected) {
        lcd.setTextColor(COLOR_RSSI_GOOD);
        lcd.drawString("WiFi", screenWidth - 5, 0);
    } else {
        lcd.setTextColor(COLOR_RSSI_POOR);
        lcd.drawString("No WiFi", screenWidth - 5, 0);
    }

    // IP letztes Byte anzeigen (rechts unten, im blauen Bereich)
    if (wifiConnected) {
        IPAddress ip = WiFi.localIP();
        lcd.setTextColor(COLOR_RSSI_GOOD);
        lcd.drawString(String(ip[3]), screenWidth - 5, 20);  // Letztes Byte der IP
    }
}

void drawSensorBox(int x, int y, int w, int h, const char* title, uint16_t color) {
    lcd.drawRoundRect(x, y, w, h, 8, color);
    lcd.setFont(&fonts::FreeSansBold12pt7b);
    lcd.setTextColor(color);
    lcd.setTextDatum(top_center);
    lcd.drawString(title, x + w / 2, y + 8);
}

void drawIndoorSection() {
    int boxX = 5;
    int boxY = is480p ? 70 : 55;
    int boxW = screenWidth / 2 - 10;
    int boxH = screenHeight - boxY - 5;

    drawSensorBox(boxX, boxY, boxW, boxH, "INDOOR", COLOR_INDOOR);

    int titleHeight = 42;
    int spriteY = boxY + titleHeight;
    int spriteW = boxW - 4;
    int spriteH = boxH - titleHeight - 2;

    indoorSprite.fillSprite(COLOR_BG);

    if (!indoorReceived) {
        // Noch nie Daten empfangen
        indoorSprite.setFont(&fonts::FreeSans9pt7b);
        indoorSprite.setTextColor(COLOR_TEXT_DIM);
        indoorSprite.setTextDatum(middle_center);
        indoorSprite.drawString("Waiting for", spriteW / 2, spriteH / 2 - 15);
        indoorSprite.drawString("sensor data", spriteW / 2, spriteH / 2 + 10);
        indoorSprite.pushSprite(boxX + 2, spriteY);
        return;
    }

    int contentY = 17;
    int lineHeight = is480p ? 35 : 28;
    int centerX = spriteW / 2;

    // Timeout-Berechnung: Master entscheidet selbst über Gültigkeit
    unsigned long secondsAgo = systemStatus.indoor_last_seen / 1000;
    bool dataValid = (systemStatus.indoor_last_seen > 0) &&
                     (secondsAgo <= (2.1 * indoorData.sleep_time_sec));

    // Temperatur (nur wenn Daten gültig)
    indoorSprite.setFont(is480p ? &fonts::FreeSansBold24pt7b : &fonts::FreeSansBold18pt7b);
    indoorSprite.setTextColor(COLOR_TEMP);
    indoorSprite.setTextDatum(middle_center);
    if (dataValid) {
        char tempStr[16];
        snprintf(tempStr, sizeof(tempStr), "%.1f C", indoorData.temperature);
        indoorSprite.drawString(tempStr, centerX, contentY);
    }
    contentY += lineHeight;

    // Luftfeuchtigkeit (nur wenn Daten gültig)
    indoorSprite.setFont(is480p ? &fonts::FreeSansBold12pt7b : &fonts::FreeSans9pt7b);
    indoorSprite.setTextColor(COLOR_HUM);
    if (dataValid) {
        indoorSprite.drawString(String(indoorData.humidity, 0) + "%", centerX, contentY);
    }
    contentY += lineHeight - 5;

    // Luftdruck (nur wenn Daten gültig)
    indoorSprite.setTextColor(COLOR_PRESS);
    if (dataValid) {
        indoorSprite.drawString(String(indoorData.pressure, 0) + " mbar", centerX, contentY);
    }
    contentY += lineHeight;

    // Batterie (immer anzeigen)
    indoorSprite.setFont(&fonts::Font2);
    uint16_t battColor = indoorData.battery_warning ? COLOR_BATTERY_LOW : COLOR_BATTERY_OK;
    indoorSprite.setTextColor(battColor);
    indoorSprite.drawString(String(indoorData.battery_mv) + " mV", centerX, contentY);
    if (indoorData.battery_warning) {
        indoorSprite.setTextColor(COLOR_BATTERY_LOW);
        indoorSprite.drawString("LOW!", centerX, contentY + 18);
    }
    contentY += lineHeight - 12;

    // RSSI (immer anzeigen)
    indoorSprite.setTextColor(getRSSIColor(indoorData.rssi));
    indoorSprite.drawString("RSSI: " + String(indoorData.rssi) + " dBm", centerX, contentY);
    contentY += lineHeight - 12;

    // Zeit seit letztem Update (immer anzeigen)
    indoorSprite.setTextColor(COLOR_TEXT_DIM);
    indoorSprite.drawString(formatTime(secondsAgo), centerX, contentY);

    indoorSprite.pushSprite(boxX + 2, spriteY);
}

void drawOutdoorSection() {
    int boxX = screenWidth / 2 + 5;
    int boxY = is480p ? 70 : 55;
    int boxW = screenWidth / 2 - 10;
    int boxH = screenHeight - boxY - 5;

    drawSensorBox(boxX, boxY, boxW, boxH, "OUTDOOR", COLOR_OUTDOOR);

    int titleHeight = 42;
    int spriteY = boxY + titleHeight;
    int spriteW = boxW - 4;
    int spriteH = boxH - titleHeight - 2;

    outdoorSprite.fillSprite(COLOR_BG);

    if (!outdoorReceived) {
        // Noch nie Daten empfangen
        outdoorSprite.setFont(&fonts::FreeSans9pt7b);
        outdoorSprite.setTextColor(COLOR_TEXT_DIM);
        outdoorSprite.setTextDatum(middle_center);
        outdoorSprite.drawString("Waiting for", spriteW / 2, spriteH / 2 - 15);
        outdoorSprite.drawString("sensor data", spriteW / 2, spriteH / 2 + 10);
        outdoorSprite.pushSprite(boxX + 2, spriteY);
        return;
    }

    int contentY = 17;
    int lineHeight = is480p ? 35 : 28;
    int centerX = spriteW / 2;

    // Timeout-Berechnung: Master entscheidet selbst über Gültigkeit
    unsigned long secondsAgo = systemStatus.outdoor_last_seen / 1000;
    bool dataValid = (systemStatus.outdoor_last_seen > 0) &&
                     (secondsAgo <= (2.1 * outdoorData.sleep_time_sec));

    // Temperatur (nur wenn Daten gültig)
    outdoorSprite.setFont(is480p ? &fonts::FreeSansBold24pt7b : &fonts::FreeSansBold18pt7b);
    outdoorSprite.setTextColor(COLOR_TEMP);
    outdoorSprite.setTextDatum(middle_center);
    if (dataValid) {
        char tempStr[16];
        snprintf(tempStr, sizeof(tempStr), "%.1f C", outdoorData.temperature);
        outdoorSprite.drawString(tempStr, centerX, contentY);
    }
    contentY += lineHeight;

    // Keine Luftfeuchtigkeit
    outdoorSprite.setFont(is480p ? &fonts::FreeSansBold12pt7b : &fonts::FreeSans9pt7b);
    outdoorSprite.setTextColor(COLOR_TEXT_DIM);
    outdoorSprite.drawString("(no humidity)", centerX, contentY);
    contentY += lineHeight - 5;

    // Luftdruck (nur wenn Daten gültig)
    outdoorSprite.setTextColor(COLOR_PRESS);
    if (dataValid) {
        outdoorSprite.drawString(String(outdoorData.pressure, 0) + " mbar", centerX, contentY);
    }
    contentY += lineHeight;

    // Batterie (immer anzeigen)
    outdoorSprite.setFont(&fonts::Font2);
    uint16_t battColor = outdoorData.battery_warning ? COLOR_BATTERY_LOW : COLOR_BATTERY_OK;
    outdoorSprite.setTextColor(battColor);
    outdoorSprite.drawString(String(outdoorData.battery_mv) + " mV", centerX, contentY);
    if (outdoorData.battery_warning) {
        outdoorSprite.setTextColor(COLOR_BATTERY_LOW);
        outdoorSprite.drawString("LOW!", centerX, contentY + 18);
    }
    contentY += lineHeight - 12;

    // RSSI (immer anzeigen)
    outdoorSprite.setTextColor(getRSSIColor(outdoorData.rssi));
    outdoorSprite.drawString("RSSI: " + String(outdoorData.rssi) + " dBm", centerX, contentY);
    contentY += lineHeight - 12;

    // Zeit seit letztem Update (immer anzeigen)
    outdoorSprite.setTextColor(COLOR_TEXT_DIM);
    outdoorSprite.drawString(formatTime(secondsAgo), centerX, contentY);

    outdoorSprite.pushSprite(boxX + 2, spriteY);
}

// ==================== MIN/MAX FUNKTIONEN ====================

void initMinMax(MinMaxData &minmax) {
    minmax.tempMin = 999.0;
    minmax.tempMax = -999.0;
    minmax.humMin = 999.0;
    minmax.humMax = -999.0;
    minmax.pressMin = 9999.0;
    minmax.pressMax = 0.0;
    minmax.batteryMin = 65535;
    minmax.batteryMax = 0;
    minmax.lastReset = millis();
}

void updateIndoorMinMax() {
    if (!indoorReceived) return;

    // Prüfen ob 24h vergangen sind
    if (millis() - indoorMinMax.lastReset > 86400000UL) {  // 24h in ms
        initMinMax(indoorMinMax);
    }

    // Min/Max aktualisieren
    if (indoorData.temperature < indoorMinMax.tempMin) indoorMinMax.tempMin = indoorData.temperature;
    if (indoorData.temperature > indoorMinMax.tempMax) indoorMinMax.tempMax = indoorData.temperature;

    if (indoorData.humidity < indoorMinMax.humMin) indoorMinMax.humMin = indoorData.humidity;
    if (indoorData.humidity > indoorMinMax.humMax) indoorMinMax.humMax = indoorData.humidity;

    if (indoorData.pressure < indoorMinMax.pressMin) indoorMinMax.pressMin = indoorData.pressure;
    if (indoorData.pressure > indoorMinMax.pressMax) indoorMinMax.pressMax = indoorData.pressure;

    if (indoorData.battery_mv < indoorMinMax.batteryMin) indoorMinMax.batteryMin = indoorData.battery_mv;
    if (indoorData.battery_mv > indoorMinMax.batteryMax) indoorMinMax.batteryMax = indoorData.battery_mv;
}

void updateOutdoorMinMax() {
    if (!outdoorReceived) return;

    // Prüfen ob 24h vergangen sind
    if (millis() - outdoorMinMax.lastReset > 86400000UL) {  // 24h in ms
        initMinMax(outdoorMinMax);
    }

    // Min/Max aktualisieren
    if (outdoorData.temperature < outdoorMinMax.tempMin) outdoorMinMax.tempMin = outdoorData.temperature;
    if (outdoorData.temperature > outdoorMinMax.tempMax) outdoorMinMax.tempMax = outdoorData.temperature;

    if (outdoorData.pressure < outdoorMinMax.pressMin) outdoorMinMax.pressMin = outdoorData.pressure;
    if (outdoorData.pressure > outdoorMinMax.pressMax) outdoorMinMax.pressMax = outdoorData.pressure;

    if (outdoorData.battery_mv < outdoorMinMax.batteryMin) outdoorMinMax.batteryMin = outdoorData.battery_mv;
    if (outdoorData.battery_mv > outdoorMinMax.batteryMax) outdoorMinMax.batteryMax = outdoorData.battery_mv;
}

// ==================== MIN/MAX DISPLAY FUNKTIONEN ====================

void drawIndoorMinMaxSection() {
    int boxX = 5;
    int boxY = is480p ? 70 : 55;
    int boxW = screenWidth / 2 - 10;
    int boxH = screenHeight - boxY - 5;

    drawSensorBox(boxX, boxY, boxW, boxH, "INDOOR", COLOR_INDOOR);

    int titleHeight = 42;
    int spriteY = boxY + titleHeight;
    int spriteW = boxW - 4;
    int spriteH = boxH - titleHeight - 2;

    indoorSprite.fillSprite(COLOR_BG);

    if (!indoorReceived || indoorMinMax.tempMin > 900) {
        indoorSprite.setFont(&fonts::FreeSans9pt7b);
        indoorSprite.setTextColor(COLOR_TEXT_DIM);
        indoorSprite.setTextDatum(middle_center);
        indoorSprite.drawString("No data yet", spriteW / 2, spriteH / 2);
        indoorSprite.pushSprite(boxX + 2, spriteY);
        return;
    }

    int contentY = 15;
    int lineHeight = is480p ? 32 : 26;

    // Temperatur Min/Max
    indoorSprite.setFont(&fonts::Font2);
    indoorSprite.setTextColor(COLOR_TEMP);
    indoorSprite.setTextDatum(middle_center);
    char tempStr[32];
    snprintf(tempStr, sizeof(tempStr), "Temp: %.1f/%.1f C", indoorMinMax.tempMin, indoorMinMax.tempMax);
    indoorSprite.drawString(tempStr, spriteW / 2, contentY);
    contentY += lineHeight;

    // Luftdruck Min/Max
    indoorSprite.setTextColor(COLOR_PRESS);
    char pressStr[32];
    snprintf(pressStr, sizeof(pressStr), "Press: %.0f/%.0f", indoorMinMax.pressMin, indoorMinMax.pressMax);
    indoorSprite.drawString(pressStr, spriteW / 2, contentY);
    contentY += lineHeight;

    // Batterie Min/Max
    indoorSprite.setTextColor(COLOR_BATTERY_OK);
    char battStr[32];
    snprintf(battStr, sizeof(battStr), "Batt: %d/%d mV", indoorMinMax.batteryMin, indoorMinMax.batteryMax);
    indoorSprite.drawString(battStr, spriteW / 2, contentY);
    contentY += lineHeight + 5;

    // Info-Text
    indoorSprite.setTextColor(COLOR_TEXT_DIM);
    indoorSprite.drawString("(24h Min/Max)", spriteW / 2, contentY);

    indoorSprite.pushSprite(boxX + 2, spriteY);
}

void drawOutdoorMinMaxSection() {
    int boxX = screenWidth / 2 + 5;
    int boxY = is480p ? 70 : 55;
    int boxW = screenWidth / 2 - 10;
    int boxH = screenHeight - boxY - 5;

    drawSensorBox(boxX, boxY, boxW, boxH, "OUTDOOR", COLOR_OUTDOOR);

    int titleHeight = 42;
    int spriteY = boxY + titleHeight;
    int spriteW = boxW - 4;
    int spriteH = boxH - titleHeight - 2;

    outdoorSprite.fillSprite(COLOR_BG);

    if (!outdoorReceived || outdoorMinMax.tempMin > 900) {
        outdoorSprite.setFont(&fonts::FreeSans9pt7b);
        outdoorSprite.setTextColor(COLOR_TEXT_DIM);
        outdoorSprite.setTextDatum(middle_center);
        outdoorSprite.drawString("No data yet", spriteW / 2, spriteH / 2);
        outdoorSprite.pushSprite(boxX + 2, spriteY);
        return;
    }

    int contentY = 15;
    int lineHeight = is480p ? 32 : 26;

    // Temperatur Min/Max
    outdoorSprite.setFont(&fonts::Font2);
    outdoorSprite.setTextColor(COLOR_TEMP);
    outdoorSprite.setTextDatum(middle_center);
    char tempStr[32];
    snprintf(tempStr, sizeof(tempStr), "Temp: %.1f/%.1f C", outdoorMinMax.tempMin, outdoorMinMax.tempMax);
    outdoorSprite.drawString(tempStr, spriteW / 2, contentY);
    contentY += lineHeight;

    // Luftdruck Min/Max
    outdoorSprite.setTextColor(COLOR_PRESS);
    char pressStr[32];
    snprintf(pressStr, sizeof(pressStr), "Press: %.0f/%.0f", outdoorMinMax.pressMin, outdoorMinMax.pressMax);
    outdoorSprite.drawString(pressStr, spriteW / 2, contentY);
    contentY += lineHeight;

    // Batterie Min/Max
    outdoorSprite.setTextColor(COLOR_BATTERY_OK);
    char battStr[32];
    snprintf(battStr, sizeof(battStr), "Batt: %d/%d mV", outdoorMinMax.batteryMin, outdoorMinMax.batteryMax);
    outdoorSprite.drawString(battStr, spriteW / 2, contentY);
    contentY += lineHeight + 5;

    // Info-Text
    outdoorSprite.setTextColor(COLOR_TEXT_DIM);
    outdoorSprite.drawString("(24h Min/Max)", spriteW / 2, contentY);

    outdoorSprite.pushSprite(boxX + 2, spriteY);
}

// ==================== GRAPH FUNKTIONEN ====================

void loadOutdoorGraphDataFromCSV() {
    if (!sdCardAvailable) {
        Serial.println("[Graph] SD card not available");
        graphData.outdoorLoaded = false;
        return;
    }

    String dateStr = getCurrentDateString();
    if (dateStr == "unknown") {
        Serial.println("[Graph] No valid time for file selection");
        graphData.outdoorLoaded = false;
        return;
    }

    const int MAX_LINES = 3000;
    float* tempData = (float*)malloc(MAX_LINES * sizeof(float));
    float* pressData = (float*)malloc(MAX_LINES * sizeof(float));
    uint16_t* batteryData = (uint16_t*)malloc(MAX_LINES * sizeof(uint16_t));
    bool* midnightData = (bool*)malloc(MAX_LINES * sizeof(bool));
    int totalLines = 0;
    int lastHour = -1;

    // Aktuelle Monatsdatei lesen
    String filename = "/" + dateStr + "_outdoor.csv";
    int currentMonthLines = 0;

    if (SD.exists(filename)) {
        File file = SD.open(filename, FILE_READ);
        if (file) {
            file.readStringUntil('\n');  // Header überspringen

            while (file.available() && currentMonthLines < MAX_LINES) {
                String line = file.readStringUntil('\n');
                int idx1 = line.indexOf(',');
                int idx2 = line.indexOf(',', idx1 + 1);
                int idx3 = line.indexOf(',', idx2 + 1);
                int idx4 = line.indexOf(',', idx3 + 1);

                if (idx1 > 0 && idx2 > 0 && idx3 > 0 && idx4 > 0) {
                    String dateTime = line.substring(0, idx1);
                    int hourIdx = dateTime.indexOf(' ') + 1;
                    int hour = dateTime.substring(hourIdx, hourIdx + 2).toInt();

                    tempData[currentMonthLines] = line.substring(idx1 + 1, idx2).toFloat();
                    pressData[currentMonthLines] = line.substring(idx2 + 1, idx3).toFloat();
                    batteryData[currentMonthLines] = line.substring(idx3 + 1, idx4).toInt();
                    midnightData[currentMonthLines] = (hour == 0 && lastHour != 0);
                    lastHour = hour;
                    currentMonthLines++;
                }
            }
            file.close();
            Serial.printf("[Graph] Current month (%s): %d lines\n", filename.c_str(), currentMonthLines);
        }
    }

    totalLines = currentMonthLines;

    // Wenn nicht genug Daten, Vormonat laden
    if (totalLines < GRAPH_DATA_POINTS) {
        String prevMonth = getPreviousMonthString(dateStr);
        String prevFilename = "/" + prevMonth + "_outdoor.csv";

        if (prevMonth != "unknown" && SD.exists(prevFilename)) {
            // Temporäre Arrays für Vormonat
            float* prevTemp = (float*)malloc(MAX_LINES * sizeof(float));
            float* prevPress = (float*)malloc(MAX_LINES * sizeof(float));
            uint16_t* prevBatt = (uint16_t*)malloc(MAX_LINES * sizeof(uint16_t));
            bool* prevMidnight = (bool*)malloc(MAX_LINES * sizeof(bool));
            int prevLines = 0;
            int prevLastHour = -1;

            File file = SD.open(prevFilename, FILE_READ);
            if (file) {
                file.readStringUntil('\n');  // Header überspringen

                while (file.available() && prevLines < MAX_LINES) {
                    String line = file.readStringUntil('\n');
                    int idx1 = line.indexOf(',');
                    int idx2 = line.indexOf(',', idx1 + 1);
                    int idx3 = line.indexOf(',', idx2 + 1);
                    int idx4 = line.indexOf(',', idx3 + 1);

                    if (idx1 > 0 && idx2 > 0 && idx3 > 0 && idx4 > 0) {
                        String dateTime = line.substring(0, idx1);
                        int hourIdx = dateTime.indexOf(' ') + 1;
                        int hour = dateTime.substring(hourIdx, hourIdx + 2).toInt();

                        prevTemp[prevLines] = line.substring(idx1 + 1, idx2).toFloat();
                        prevPress[prevLines] = line.substring(idx2 + 1, idx3).toFloat();
                        prevBatt[prevLines] = line.substring(idx3 + 1, idx4).toInt();
                        prevMidnight[prevLines] = (hour == 0 && prevLastHour != 0);
                        prevLastHour = hour;
                        prevLines++;
                    }
                }
                file.close();

                // Fehlende Anzahl aus Vormonat nehmen
                int needed = GRAPH_DATA_POINTS - currentMonthLines;
                int prevStart = (prevLines > needed) ? (prevLines - needed) : 0;
                int prevCount = prevLines - prevStart;

                // Aktuelle Daten nach hinten verschieben
                for (int i = currentMonthLines - 1; i >= 0; i--) {
                    tempData[i + prevCount] = tempData[i];
                    pressData[i + prevCount] = pressData[i];
                    batteryData[i + prevCount] = batteryData[i];
                    midnightData[i + prevCount] = midnightData[i];
                }

                // Vormonatsdaten an den Anfang kopieren
                for (int i = 0; i < prevCount; i++) {
                    tempData[i] = prevTemp[prevStart + i];
                    pressData[i] = prevPress[prevStart + i];
                    batteryData[i] = prevBatt[prevStart + i];
                    midnightData[i] = prevMidnight[prevStart + i];
                }

                totalLines = prevCount + currentMonthLines;
                Serial.printf("[Graph] Added %d lines from previous month (%s)\n", prevCount, prevFilename.c_str());
            }

            free(prevMidnight);
            free(prevBatt);
            free(prevPress);
            free(prevTemp);
        }
    }

    // Die letzten 240 Werte ins graphData kopieren
    int startIdx = (totalLines > GRAPH_DATA_POINTS) ? (totalLines - GRAPH_DATA_POINTS) : 0;
    graphData.dataCount = totalLines - startIdx;

    for (int i = 0; i < graphData.dataCount; i++) {
        graphData.outdoorTempValues[i] = tempData[startIdx + i];
        graphData.outdoorPressValues[i] = pressData[startIdx + i];
        graphData.outdoorBatteryValues[i] = batteryData[startIdx + i];
        graphData.midnightMarker[i] = midnightData[startIdx + i];
    }

    free(midnightData);
    free(batteryData);
    free(pressData);
    free(tempData);

    graphData.outdoorLoaded = true;
    Serial.printf("[Graph] Loaded %d outdoor data points (total)\n", graphData.dataCount);
}

void loadIndoorGraphDataFromCSV() {
    if (!sdCardAvailable) {
        Serial.println("[Graph] SD card not available");
        graphData.indoorLoaded = false;
        return;
    }

    String dateStr = getCurrentDateString();
    if (dateStr == "unknown") {
        Serial.println("[Graph] No valid time for file selection");
        graphData.indoorLoaded = false;
        return;
    }

    const int MAX_LINES = 3000;
    uint16_t* batteryData = (uint16_t*)malloc(MAX_LINES * sizeof(uint16_t));
    int totalLines = 0;

    // Aktuelle Monatsdatei lesen
    String filename = "/" + dateStr + "_indoor.csv";
    int currentMonthLines = 0;

    if (SD.exists(filename)) {
        File file = SD.open(filename, FILE_READ);
        if (file) {
            file.readStringUntil('\n');  // Header überspringen

            while (file.available() && currentMonthLines < MAX_LINES) {
                String line = file.readStringUntil('\n');
                int idx1 = line.indexOf(',');
                int idx2 = line.indexOf(',', idx1 + 1);
                int idx3 = line.indexOf(',', idx2 + 1);
                int idx4 = line.indexOf(',', idx3 + 1);
                int idx5 = line.indexOf(',', idx4 + 1);

                if (idx1 > 0 && idx5 > 0) {
                    batteryData[currentMonthLines] = line.substring(idx4 + 1, idx5).toInt();
                    currentMonthLines++;
                }
            }
            file.close();
            Serial.printf("[Graph] Current month (%s): %d lines\n", filename.c_str(), currentMonthLines);
        }
    }

    totalLines = currentMonthLines;

    // Wenn nicht genug Daten, Vormonat laden
    if (totalLines < GRAPH_DATA_POINTS) {
        String prevMonth = getPreviousMonthString(dateStr);
        String prevFilename = "/" + prevMonth + "_indoor.csv";

        if (prevMonth != "unknown" && SD.exists(prevFilename)) {
            uint16_t* prevBatt = (uint16_t*)malloc(MAX_LINES * sizeof(uint16_t));
            int prevLines = 0;

            File file = SD.open(prevFilename, FILE_READ);
            if (file) {
                file.readStringUntil('\n');  // Header überspringen

                while (file.available() && prevLines < MAX_LINES) {
                    String line = file.readStringUntil('\n');
                    int idx1 = line.indexOf(',');
                    int idx2 = line.indexOf(',', idx1 + 1);
                    int idx3 = line.indexOf(',', idx2 + 1);
                    int idx4 = line.indexOf(',', idx3 + 1);
                    int idx5 = line.indexOf(',', idx4 + 1);

                    if (idx1 > 0 && idx5 > 0) {
                        prevBatt[prevLines] = line.substring(idx4 + 1, idx5).toInt();
                        prevLines++;
                    }
                }
                file.close();

                // Fehlende Anzahl aus Vormonat nehmen
                int needed = GRAPH_DATA_POINTS - currentMonthLines;
                int prevStart = (prevLines > needed) ? (prevLines - needed) : 0;
                int prevCount = prevLines - prevStart;

                // Aktuelle Daten nach hinten verschieben
                for (int i = currentMonthLines - 1; i >= 0; i--) {
                    batteryData[i + prevCount] = batteryData[i];
                }

                // Vormonatsdaten an den Anfang kopieren
                for (int i = 0; i < prevCount; i++) {
                    batteryData[i] = prevBatt[prevStart + i];
                }

                totalLines = prevCount + currentMonthLines;
                Serial.printf("[Graph] Added %d lines from previous month (%s)\n", prevCount, prevFilename.c_str());
            }

            free(prevBatt);
        }
    }

    // Die letzten 240 Werte ins graphData kopieren
    int startIdx = (totalLines > GRAPH_DATA_POINTS) ? (totalLines - GRAPH_DATA_POINTS) : 0;
    int count = totalLines - startIdx;

    // Synchronisiere mit Outdoor dataCount (beide Sensoren sollten gleiche Anzahl haben)
    for (int i = 0; i < count && i < graphData.dataCount; i++) {
        graphData.indoorBatteryValues[i] = batteryData[startIdx + i];
    }

    free(batteryData);

    graphData.indoorLoaded = true;
    Serial.printf("[Graph] Loaded %d indoor battery values (total)\n", count);
}

void drawOutdoorGraphSection() {
    lcd.fillScreen(COLOR_BG);

    lcd.setFont(&fonts::FreeSansBold12pt7b);
    lcd.setTextColor(COLOR_OUTDOOR);
    lcd.setTextDatum(top_center);
    lcd.drawString("OUTDOOR - Last 240 Values", screenWidth / 2, 5);

    if (!graphData.outdoorLoaded || graphData.dataCount < 2) {
        lcd.setFont(&fonts::FreeSans9pt7b);
        lcd.setTextColor(COLOR_TEXT_DIM);
        lcd.drawString("No data available", screenWidth / 2, screenHeight / 2);
        return;
    }

    int graphX = 40;
    int graphY = 35;
    int graphW = screenWidth - 50;
    int graphH = (screenHeight - 45) / 2;

    // ==== TEMPERATUR GRAPH ====
    float tempMin = graphData.outdoorTempValues[0];
    float tempMax = graphData.outdoorTempValues[0];
    for (int i = 1; i < graphData.dataCount; i++) {
        if (graphData.outdoorTempValues[i] < tempMin) tempMin = graphData.outdoorTempValues[i];
        if (graphData.outdoorTempValues[i] > tempMax) tempMax = graphData.outdoorTempValues[i];
    }

    float tempRange = tempMax - tempMin;
    if (tempRange < 1.0) tempRange = 1.0;
    tempMin -= tempRange * 0.1;
    tempMax += tempRange * 0.1;

    lcd.drawRect(graphX, graphY, graphW, graphH, COLOR_TEMP);

    lcd.setFont(&fonts::Font2);
    lcd.setTextColor(COLOR_TEMP);
    lcd.setTextDatum(middle_right);
    lcd.drawString(String(tempMax, 1), graphX - 3, graphY + 5);
    lcd.drawString(String(tempMin, 1), graphX - 3, graphY + graphH - 5);
    lcd.drawString("C", graphX - 3, graphY + graphH / 2);

    // Mitternachts-Markierungen
    for (int i = 0; i < graphData.dataCount; i++) {
        if (graphData.midnightMarker[i]) {
            int x = graphX + i * graphW / (graphData.dataCount - 1);
            lcd.drawFastVLine(x, graphY + 1, graphH - 2, COLOR_TEXT_DIM);
        }
    }

    // Kurve
    lcd.setColor(COLOR_TEMP);
    for (int i = 1; i < graphData.dataCount; i++) {
        int x1 = graphX + (i - 1) * graphW / (graphData.dataCount - 1);
        int x2 = graphX + i * graphW / (graphData.dataCount - 1);
        int y1 = graphY + graphH - (int)((graphData.outdoorTempValues[i - 1] - tempMin) / (tempMax - tempMin) * graphH);
        int y2 = graphY + graphH - (int)((graphData.outdoorTempValues[i] - tempMin) / (tempMax - tempMin) * graphH);
        lcd.drawLine(x1, y1, x2, y2);
    }

    // ==== LUFTDRUCK GRAPH ====
    int pressGraphY = graphY + graphH + 10;

    float pressMin = graphData.outdoorPressValues[0];
    float pressMax = graphData.outdoorPressValues[0];
    for (int i = 1; i < graphData.dataCount; i++) {
        if (graphData.outdoorPressValues[i] < pressMin) pressMin = graphData.outdoorPressValues[i];
        if (graphData.outdoorPressValues[i] > pressMax) pressMax = graphData.outdoorPressValues[i];
    }

    float pressRange = pressMax - pressMin;
    if (pressRange < 5.0) pressRange = 5.0;
    pressMin -= pressRange * 0.1;
    pressMax += pressRange * 0.1;

    lcd.drawRect(graphX, pressGraphY, graphW, graphH, COLOR_PRESS);

    lcd.setTextColor(COLOR_PRESS);
    lcd.setTextDatum(middle_right);
    lcd.drawString(String((int)pressMax), graphX - 3, pressGraphY + 5);
    lcd.drawString(String((int)pressMin), graphX - 3, pressGraphY + graphH - 5);
    lcd.drawString("mbar", graphX - 3, pressGraphY + graphH / 2);

    // Mitternachts-Markierungen
    for (int i = 0; i < graphData.dataCount; i++) {
        if (graphData.midnightMarker[i]) {
            int x = graphX + i * graphW / (graphData.dataCount - 1);
            lcd.drawFastVLine(x, pressGraphY + 1, graphH - 2, COLOR_TEXT_DIM);
        }
    }

    // Kurve
    lcd.setColor(COLOR_PRESS);
    for (int i = 1; i < graphData.dataCount; i++) {
        int x1 = graphX + (i - 1) * graphW / (graphData.dataCount - 1);
        int x2 = graphX + i * graphW / (graphData.dataCount - 1);
        int y1 = pressGraphY + graphH - (int)((graphData.outdoorPressValues[i - 1] - pressMin) / (pressMax - pressMin) * graphH);
        int y2 = pressGraphY + graphH - (int)((graphData.outdoorPressValues[i] - pressMin) / (pressMax - pressMin) * graphH);
        lcd.drawLine(x1, y1, x2, y2);
    }

    lcd.setFont(&fonts::Font2);
    lcd.setTextColor(COLOR_TEXT_DIM);
    lcd.setTextDatum(bottom_center);
    lcd.drawString(String(graphData.dataCount) + " values from CSV", screenWidth / 2, screenHeight - 2);
}

void drawBatteryGraphSection() {
    lcd.fillScreen(COLOR_BG);

    lcd.setFont(&fonts::FreeSansBold12pt7b);
    lcd.setTextColor(COLOR_BATTERY_OK);
    lcd.setTextDatum(top_center);
    lcd.drawString("BATTERY - Last 240 Values", screenWidth / 2, 5);

    if ((!graphData.outdoorLoaded && !graphData.indoorLoaded) || graphData.dataCount < 2) {
        lcd.setFont(&fonts::FreeSans9pt7b);
        lcd.setTextColor(COLOR_TEXT_DIM);
        lcd.drawString("No data available", screenWidth / 2, screenHeight / 2);
        return;
    }

    int graphX = 40;
    int graphY = 40;
    int graphW = screenWidth - 50;
    int graphH = screenHeight - 60;

    // Min/Max finden (über beide Sensoren)
    uint16_t battMin = 65535;
    uint16_t battMax = 0;

    if (graphData.outdoorLoaded) {
        for (int i = 0; i < graphData.dataCount; i++) {
            if (graphData.outdoorBatteryValues[i] < battMin) battMin = graphData.outdoorBatteryValues[i];
            if (graphData.outdoorBatteryValues[i] > battMax) battMax = graphData.outdoorBatteryValues[i];
        }
    }

    if (graphData.indoorLoaded) {
        for (int i = 0; i < graphData.dataCount; i++) {
            if (graphData.indoorBatteryValues[i] < battMin) battMin = graphData.indoorBatteryValues[i];
            if (graphData.indoorBatteryValues[i] > battMax) battMax = graphData.indoorBatteryValues[i];
        }
    }

    // Margin
    uint16_t battRange = battMax - battMin;
    if (battRange < 100) battRange = 100;
    battMin -= battRange * 0.1;
    battMax += battRange * 0.1;

    // Rahmen
    lcd.drawRect(graphX, graphY, graphW, graphH, COLOR_BATTERY_OK);

    // Y-Achsen Beschriftung
    lcd.setFont(&fonts::Font2);
    lcd.setTextColor(COLOR_BATTERY_OK);
    lcd.setTextDatum(middle_right);
    lcd.drawString(String(battMax), graphX - 3, graphY + 5);
    lcd.drawString(String(battMin), graphX - 3, graphY + graphH - 5);
    lcd.drawString("mV", graphX - 3, graphY + graphH / 2);

    // Mitternachts-Markierungen
    for (int i = 0; i < graphData.dataCount; i++) {
        if (graphData.midnightMarker[i]) {
            int x = graphX + i * graphW / (graphData.dataCount - 1);
            lcd.drawFastVLine(x, graphY + 1, graphH - 2, COLOR_TEXT_DIM);
        }
    }

    // Outdoor Kurve (Orange)
    if (graphData.outdoorLoaded) {
        lcd.setColor(COLOR_OUTDOOR);
        for (int i = 1; i < graphData.dataCount; i++) {
            int x1 = graphX + (i - 1) * graphW / (graphData.dataCount - 1);
            int x2 = graphX + i * graphW / (graphData.dataCount - 1);
            int y1 = graphY + graphH - (int)((graphData.outdoorBatteryValues[i - 1] - battMin) / (float)(battMax - battMin) * graphH);
            int y2 = graphY + graphH - (int)((graphData.outdoorBatteryValues[i] - battMin) / (float)(battMax - battMin) * graphH);
            lcd.drawLine(x1, y1, x2, y2);
        }
    }

    // Indoor Kurve (Cyan)
    if (graphData.indoorLoaded) {
        lcd.setColor(COLOR_INDOOR);
        for (int i = 1; i < graphData.dataCount; i++) {
            int x1 = graphX + (i - 1) * graphW / (graphData.dataCount - 1);
            int x2 = graphX + i * graphW / (graphData.dataCount - 1);
            int y1 = graphY + graphH - (int)((graphData.indoorBatteryValues[i - 1] - battMin) / (float)(battMax - battMin) * graphH);
            int y2 = graphY + graphH - (int)((graphData.indoorBatteryValues[i] - battMin) / (float)(battMax - battMin) * graphH);
            lcd.drawLine(x1, y1, x2, y2);
        }
    }

    // Legende
    lcd.setFont(&fonts::Font2);
    lcd.setTextDatum(bottom_left);
    lcd.setTextColor(COLOR_OUTDOOR);
    lcd.drawString("Outdoor", graphX + 5, screenHeight - 2);
    lcd.setTextColor(COLOR_INDOOR);
    lcd.drawString("Indoor", graphX + 70, screenHeight - 2);

    lcd.setTextColor(COLOR_TEXT_DIM);
    lcd.setTextDatum(bottom_right);
    lcd.drawString(String(graphData.dataCount) + " values", screenWidth - 5, screenHeight - 2);
}

// ==================== TOUCH FUNKTIONEN ====================

void checkTouch() {
    int16_t touchX, touchY;

    // Touch prüfen - getTouch() gibt true zurück wenn Touch erkannt wurde
    // Die Touch-Konfiguration kommt aus CYD_Display_Config.h
    if (lcd.getTouch(&touchX, &touchY)) {

        // Debounce
        if (millis() - lastTouchTime > TOUCH_DEBOUNCE) {
            lastTouchTime = millis();
            lastModeChange = millis();  // Reset Auto-Return Timer

            // Display-Modus umschalten (zyklisch: 0->1->2->3->0)
            displayMode = (displayMode + 1) % 4;

            const char* modeNames[] = {"Normal", "Outdoor Graph", "Battery Graph", "Min/Max"};
            Serial.printf("[Touch] Mode switched to: %s (at X=%d, Y=%d)\n",
                         modeNames[displayMode], touchX, touchY);

            // Graph-Daten bei jedem Wechsel neu laden (zeigt aktuelle Daten)
            if (displayMode == 1) {
                loadOutdoorGraphDataFromCSV();
            } else if (displayMode == 2) {
                // Beide Sensoren neu laden für aktuellste Daten
                loadOutdoorGraphDataFromCSV();
                loadIndoorGraphDataFromCSV();
            }

            // Display sofort aktualisieren
            updateDisplay();
        }
    }
}

void updateDisplay() {
    if (displayMode == 0) {
        // Normal - mit Header
        lcd.fillScreen(COLOR_BG);
        drawHeader();
        if (indoorReceived) drawIndoorSection();
        if (outdoorReceived) drawOutdoorSection();
    } else if (displayMode == 1) {
        // Outdoor Graph - ohne Header (vollbild)
        drawOutdoorGraphSection();
    } else if (displayMode == 2) {
        // Battery Graph - ohne Header (vollbild)
        drawBatteryGraphSection();
    } else if (displayMode == 3) {
        // Min/Max - mit Header
        lcd.fillScreen(COLOR_BG);
        drawHeader();
        drawIndoorMinMaxSection();
        drawOutdoorMinMaxSection();
    }
}

void checkAutoReturn() {
    // Nach 30 Sekunden automatisch zurück zu Schirm 1 (nur wenn nicht schon dort)
    if (displayMode != 0 && (millis() - lastModeChange > AUTO_RETURN_TIME)) {
        Serial.println("[Auto-Return] Returning to normal view after 30 seconds");
        displayMode = 0;
        lastModeChange = millis();
        updateDisplay();
    }
}

// ==================== I2C FUNKTIONEN ====================

void pollI2CData() {
    // Bridge 1 abfragen
    if (!i2cBridge.ping(BRIDGE_ADDRESS_1)) {
        Serial.println("[I2C] Bridge not responding!");
        return;
    }
    
    // Neue Daten prüfen
    uint8_t newDataMask = i2cBridge.checkNewData(BRIDGE_ADDRESS_1);
    
    if (newDataMask == 0) {
        return; // Keine neuen Daten
    }
    
    Serial.printf("[I2C] New data available: 0x%02X\n", newDataMask);
    
    // Indoor Daten (ID 0x01)
    if (newDataMask & 0x02) {  // Bit 1 für ID 0x01
        if (i2cBridge.readStruct(BRIDGE_ADDRESS_1, 0x01, indoorData)) {
            indoorReceived = true;
            lastIndoorUpdate = millis();

            // Min/Max aktualisieren
            updateIndoorMinMax();

            Serial.printf("[Indoor] Temp: %.1f°C, Hum: %.1f%%, Press: %.0f mbar\n",
                         indoorData.temperature, indoorData.humidity, indoorData.pressure);
        }
    }

    // Outdoor Daten (ID 0x02)
    if (newDataMask & 0x04) {  // Bit 2 für ID 0x02
        if (i2cBridge.readStruct(BRIDGE_ADDRESS_1, 0x02, outdoorData)) {
            outdoorReceived = true;
            lastOutdoorUpdate = millis();

            // Min/Max aktualisieren
            updateOutdoorMinMax();

            Serial.printf("[Outdoor] Temp: %.1f°C, Press: %.0f mbar\n",
                         outdoorData.temperature, outdoorData.pressure);
        }
    }
    
    // System Status (ID 0x03)
    if (newDataMask & 0x08) {  // Bit 3 für ID 0x03
        if (i2cBridge.readStruct(BRIDGE_ADDRESS_1, 0x03, systemStatus)) {
            Serial.printf("[Status] Indoor: %lu ms ago, Outdoor: %lu ms ago, Packets: %d\n",
                         systemStatus.indoor_last_seen,
                         systemStatus.outdoor_last_seen,
                         systemStatus.esp_now_packets);
        }
    }
}

// ==================== WiFi & NTP ====================

void setupWiFi() {
    Serial.println("\n[WiFi] Connecting to " + String(ssid));
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    
    // Non-blocking connect (max 10 Sekunden)
    unsigned long startAttempt = millis();
    
    while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 10000) {
        delay(500);
        Serial.print(".");
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        Serial.println("\n[WiFi] Connected!");
        Serial.print("[WiFi] IP: ");
        Serial.println(WiFi.localIP());
        
        // NTP konfigurieren
        configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
        timeConfigured = true;
        Serial.println("[NTP] Time configured");
    } else {
        Serial.println("\n[WiFi] Connection failed - will retry later");
        wifiConnected = false;
    }
}

// ==================== INFLUXDB FUNKTIONEN ====================

#ifdef ENABLE_INFLUXDB

bool setupInfluxDB() {
    Serial.println("\n[InfluxDB] Initializing...");

    // Timezone setzen
    influxClient.setConnectionParamsV1(INFLUXDB_URL, INFLUXDB_BUCKET, INFLUXDB_ORG, INFLUXDB_TOKEN);

    // Server-Validierung
    if (influxClient.validateConnection()) {
        Serial.print("[InfluxDB] Connected to: ");
        Serial.println(influxClient.getServerUrl());
        return true;
    } else {
        Serial.print("[InfluxDB] Connection failed: ");
        Serial.println(influxClient.getLastErrorMessage());
        return false;
    }
}

void sendIndoorToInfluxDB() {
    if (!wifiConnected || !influxDBConnected || !indoorReceived) return;

    // Data Point erstellen
    Point indoorPoint("indoor_sensor");

    // Tags (für Filterung/Gruppierung)
    indoorPoint.addTag("device", "CYD_Master");
    indoorPoint.addTag("location", "indoor");
    indoorPoint.addTag("sensor_type", "ESP8266");

    // Fields (die eigentlichen Messwerte)
    indoorPoint.addField("temperature", indoorData.temperature);
    indoorPoint.addField("humidity", indoorData.humidity);
    indoorPoint.addField("pressure", indoorData.pressure);
    indoorPoint.addField("battery_mv", (int)indoorData.battery_mv);
    indoorPoint.addField("rssi", (int)indoorData.rssi);
    indoorPoint.addField("battery_warning", indoorData.battery_warning ? 1 : 0);
    indoorPoint.addField("sleep_time_sec", (int)indoorData.sleep_time_sec);

    // Daten senden
    if (!influxClient.writePoint(indoorPoint)) {
        Serial.print("[InfluxDB] Write Indoor failed: ");
        Serial.println(influxClient.getLastErrorMessage());
    } else {
        Serial.println("[InfluxDB] Indoor data written");
    }
}

void sendOutdoorToInfluxDB() {
    if (!wifiConnected || !influxDBConnected || !outdoorReceived) return;

    // Data Point erstellen
    Point outdoorPoint("outdoor_sensor");

    // Tags
    outdoorPoint.addTag("device", "CYD_Master");
    outdoorPoint.addTag("location", "outdoor");
    outdoorPoint.addTag("sensor_type", "ESP8266");

    // Fields
    outdoorPoint.addField("temperature", outdoorData.temperature);
    outdoorPoint.addField("pressure", outdoorData.pressure);
    outdoorPoint.addField("battery_mv", (int)outdoorData.battery_mv);
    outdoorPoint.addField("rssi", (int)outdoorData.rssi);
    outdoorPoint.addField("battery_warning", outdoorData.battery_warning ? 1 : 0);
    outdoorPoint.addField("sleep_time_sec", (int)outdoorData.sleep_time_sec);

    // Daten senden
    if (!influxClient.writePoint(outdoorPoint)) {
        Serial.print("[InfluxDB] Write Outdoor failed: ");
        Serial.println(influxClient.getLastErrorMessage());
    } else {
        Serial.println("[InfluxDB] Outdoor data written");
    }
}

#endif

// ==================== SD-KARTE FUNKTIONEN ====================

bool initSDCard() {
    Serial.println("\n[SD] Initializing SD Card...");

    // Eigenen SPI-Bus für SD-Karte initialisieren (VSPI)
    // Display verwendet eigenen SPI (Pins 14,13,12)
    // Touch verwendet eigenen SPI (Pins 25,32,39)
    // SD-Karte bekommt VSPI (Pins 18,19,23)
    Serial.printf("[SD] SPI Pins: SCK=%d, MISO=%d, MOSI=%d, CS=%d\n", SD_SCK, SD_MISO, SD_MOSI, SD_CS);

    // SPI.begin Parameter: SCK, MISO, MOSI (KEIN CS!)
    sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI);

    Serial.println("[SD] Calling SD.begin() with dedicated SPI bus...");
    if (!SD.begin(SD_CS, sdSPI)) {
        Serial.println("[SD] ERROR: No SD Card found or initialization failed");
        Serial.println("[SD] Check: 1) Card inserted? 2) Wiring correct? 3) Card formatted as FAT32?");
        return false;
    }

    Serial.println("[SD] SD.begin() successful");
    uint8_t cardType = SD.cardType();

    if (cardType == CARD_NONE) {
        Serial.println("[SD] ERROR: No SD card attached (cardType == CARD_NONE)");
        return false;
    }

    Serial.print("[SD] Card Type: ");
    if (cardType == CARD_MMC) {
        Serial.println("MMC");
    } else if (cardType == CARD_SD) {
        Serial.println("SDSC");
    } else if (cardType == CARD_SDHC) {
        Serial.println("SDHC");
    } else {
        Serial.println("UNKNOWN");
    }

    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    Serial.printf("[SD] Card Size: %llu MB\n", cardSize);
    Serial.printf("[SD] Used Space: %llu MB\n", SD.usedBytes() / (1024 * 1024));
    Serial.println("[SD] SD Card successfully initialized!");

    return true;
}

String getCurrentDateString() {
    if (!timeConfigured) return "unknown";

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) return "unknown";

    char dateStr[16];
    strftime(dateStr, sizeof(dateStr), "%Y%m", &timeinfo);  // Nur Jahr+Monat (monatliche Dateien)
    return String(dateStr);
}

String getPreviousMonthString(String currentMonth) {
    // Format: YYYYMM (z.B. "202501" → "202412")
    if (currentMonth == "unknown" || currentMonth.length() != 6) return "unknown";

    int year = currentMonth.substring(0, 4).toInt();
    int month = currentMonth.substring(4, 6).toInt();

    // Vormonat berechnen
    if (month == 1) {
        year--;
        month = 12;
    } else {
        month--;
    }

    char prevMonth[16];
    snprintf(prevMonth, sizeof(prevMonth), "%04d%02d", year, month);
    return String(prevMonth);
}

String getDateTimeString() {
    if (!timeConfigured) return "N/A";

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) return "N/A";

    char dateTimeStr[32];
    strftime(dateTimeStr, sizeof(dateTimeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
    return String(dateTimeStr);
}

void logIndoorData() {
    if (!sdCardAvailable || !indoorReceived) return;

    String dateStr = getCurrentDateString();
    if (dateStr == "unknown") return; // Keine gültige Zeit

    String filename = "/" + dateStr + "_indoor.csv";
    bool fileExists = SD.exists(filename);

    File file = SD.open(filename, FILE_APPEND);
    if (!file) {
        Serial.println("[SD] Failed to open indoor log file");
        return;
    }

    // Header schreiben, falls neue Datei
    if (!fileExists) {
        file.println("DateTime,Temperature_C,Humidity_%,Pressure_mbar,Battery_mV,RSSI_dBm,Battery_Warning,Sleep_Time_sec");
    }

    // Daten schreiben
    file.print(getDateTimeString());
    file.print(",");
    file.print(indoorData.temperature, 1);
    file.print(",");
    file.print(indoorData.humidity, 1);
    file.print(",");
    file.print(indoorData.pressure, 0);
    file.print(",");
    file.print(indoorData.battery_mv);
    file.print(",");
    file.print(indoorData.rssi);
    file.print(",");
    file.print(indoorData.battery_warning ? "1" : "0");
    file.print(",");
    file.println(indoorData.sleep_time_sec);

    file.close();
    Serial.printf("[SD] Indoor data logged to %s\n", filename.c_str());
}

void logOutdoorData() {
    if (!sdCardAvailable || !outdoorReceived) return;

    String dateStr = getCurrentDateString();
    if (dateStr == "unknown") return; // Keine gültige Zeit

    String filename = "/" + dateStr + "_outdoor.csv";
    bool fileExists = SD.exists(filename);

    File file = SD.open(filename, FILE_APPEND);
    if (!file) {
        Serial.println("[SD] Failed to open outdoor log file");
        return;
    }

    // Header schreiben, falls neue Datei
    if (!fileExists) {
        file.println("DateTime,Temperature_C,Pressure_mbar,Battery_mV,RSSI_dBm,Battery_Warning,Sleep_Time_sec");
    }

    // Daten schreiben
    file.print(getDateTimeString());
    file.print(",");
    file.print(outdoorData.temperature, 1);
    file.print(",");
    file.print(outdoorData.pressure, 0);
    file.print(",");
    file.print(outdoorData.battery_mv);
    file.print(",");
    file.print(outdoorData.rssi);
    file.print(",");
    file.print(outdoorData.battery_warning ? "1" : "0");
    file.print(",");
    file.println(outdoorData.sleep_time_sec);

    file.close();
    Serial.printf("[SD] Outdoor data logged to %s\n", filename.c_str());
}

// ==================== WEBSERVER FUNKTIONEN ====================

void handleRoot() {
    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<title>CYD Sensor Logger</title>";
    html += "<style>";
    html += "body { font-family: Arial, sans-serif; margin: 20px; background: #f0f0f0; }";
    html += "h1 { color: #333; }";
    html += ".container { background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 5px rgba(0,0,0,0.1); }";
    html += ".file-list { margin-top: 20px; }";
    html += ".file-item { padding: 10px; margin: 5px 0; background: #e8f4f8; border-radius: 5px; }";
    html += ".file-item a { color: #0066cc; text-decoration: none; font-weight: bold; }";
    html += ".file-item a:hover { text-decoration: underline; }";
    html += ".status { padding: 10px; margin: 10px 0; border-radius: 5px; }";
    html += ".status.ok { background: #d4edda; color: #155724; }";
    html += ".status.error { background: #f8d7da; color: #721c24; }";
    html += ".info { color: #666; font-size: 0.9em; }";
    html += "</style></head><body>";
    html += "<div class='container'>";
    html += "<h1>🌡️ CYD Sensor Datenlogger</h1>";

    if (!sdCardAvailable) {
        html += "<div class='status error'>❌ Keine SD-Karte gefunden</div>";
    } else {
        html += "<div class='status ok'>✅ SD-Karte aktiv</div>";
        html += "<p class='info'>Speicherplatz: " + String(SD.usedBytes() / 1024) + " KB belegt von " + String(SD.cardSize() / (1024 * 1024)) + " MB</p>";

        html += "<h2>📁 Verfügbare Log-Dateien:</h2>";
        html += "<div class='file-list'>";

        File root = SD.open("/");
        File file = root.openNextFile();
        bool hasFiles = false;

        while (file) {
            if (!file.isDirectory()) {
                String filename = String(file.name());
                if (filename.endsWith(".csv")) {
                    hasFiles = true;
                    html += "<div class='file-item'>";
                    html += "📄 <a href='/download?file=" + filename + "'>" + filename + "</a>";
                    html += " <span class='info'>(" + String(file.size() / 1024) + " KB)</span>";
                    html += "</div>";
                }
            }
            file = root.openNextFile();
        }

        if (!hasFiles) {
            html += "<p class='info'>Noch keine Log-Dateien vorhanden.</p>";
        }

        html += "</div>";
    }

    html += "<h2>📊 Aktuelle Messwerte:</h2>";

    if (indoorReceived) {
        html += "<h3 style='color: #0099cc;'>🏠 Indoor</h3>";
        html += "<p>Temperatur: " + String(indoorData.temperature, 1) + " °C<br>";
        html += "Luftfeuchtigkeit: " + String(indoorData.humidity, 0) + " %<br>";
        html += "Luftdruck: " + String(indoorData.pressure, 0) + " mbar<br>";
        html += "Batterie: " + String(indoorData.battery_mv) + " mV</p>";
    }

    if (outdoorReceived) {
        html += "<h3 style='color: #ff9900;'>🌤️ Outdoor</h3>";
        html += "<p>Temperatur: " + String(outdoorData.temperature, 1) + " °C<br>";
        html += "Luftdruck: " + String(outdoorData.pressure, 0) + " mbar<br>";
        html += "Batterie: " + String(outdoorData.battery_mv) + " mV</p>";
    }

    html += "<p class='info' style='margin-top: 30px;'>Aktualisiert: " + getDateTimeString() + "</p>";
    html += "</div></body></html>";

    server.send(200, "text/html", html);
}

void handleDownload() {
    if (!server.hasArg("file")) {
        server.send(400, "text/plain", "Missing file parameter");
        return;
    }

    String filename = server.arg("file");

    // Sicherheit: Nur .csv Dateien erlauben
    if (!filename.endsWith(".csv")) {
        server.send(403, "text/plain", "Only CSV files allowed");
        return;
    }

    // Sicherheit: Kein Pfad-Traversal
    if (filename.indexOf("..") >= 0 || filename.indexOf("/") > 0) {
        server.send(403, "text/plain", "Invalid filename");
        return;
    }

    String filepath = "/" + filename;

    if (!SD.exists(filepath)) {
        server.send(404, "text/plain", "File not found");
        return;
    }

    File file = SD.open(filepath, FILE_READ);
    if (!file) {
        server.send(500, "text/plain", "Failed to open file");
        return;
    }

    server.streamFile(file, "text/csv");
    file.close();
}

void setupWebServer() {
    server.on("/", handleRoot);
    server.on("/download", handleDownload);
    server.begin();
    Serial.println("[Web] Server started on http://" + WiFi.localIP().toString());
}

// ==================== SETUP ====================

void setup() {
    Serial.begin(115200);
    Serial.println("\n\n===========================================");
    Serial.println("        CYD I2C Sensor Display");
    Serial.println("===========================================\n");
    
    // ========== Display initialisieren ==========
    Serial.println("[Display] Initializing...");
    
    lcd.init();
    lcd.setRotation(DISPLAY_ROTATION);
    lcd.setBrightness(128);

    screenWidth = lcd.width();
    screenHeight = lcd.height();
    is480p = (screenWidth == 480 || screenHeight == 480);

    Serial.printf("[Display] Size: %dx%d\n", screenWidth, screenHeight);
    Serial.printf("[Display] Rotation: %d\n", DISPLAY_ROTATION);
    Serial.println("[Touch] Touch configured via CYD_Display_Config.h");
    
    lcd.fillScreen(COLOR_BG);
    drawHeader();
    
    // Sprites erstellen
    int boxY = is480p ? 70 : 55;
    int boxH = screenHeight - boxY - 5;
    int boxW = screenWidth / 2 - 10;
    int titleHeight = 42;
    int spriteW = boxW - 4;
    int spriteH = boxH - titleHeight - 2;
    
    indoorSprite.createSprite(spriteW, spriteH);
    outdoorSprite.createSprite(spriteW, spriteH);
    
    Serial.printf("[Display] Sprites created: %dx%d\n", spriteW, spriteH);
    
    // ========== I2C Bridge initialisieren ==========
    Serial.println("\n[I2C] Initializing master...");
    
    i2cBridge.beginMaster(extSDA, extSCL, I2C_FREQUENCY);
    
    // Structs registrieren (für lokale Verwaltung)
    i2cBridge.registerStruct(0x01, &indoorData, 1, "Indoor");
    i2cBridge.registerStruct(0x02, &outdoorData, 1, "Outdoor");
    i2cBridge.registerStruct(0x03, &systemStatus, 1, "Status");
    
    Serial.printf("[I2C] Master mode on SDA=%d, SCL=%d\n", extSDA, extSCL);
    Serial.printf("[I2C] Scanning for bridge at 0x%02X...\n", BRIDGE_ADDRESS_1);
    
    if (i2cBridge.ping(BRIDGE_ADDRESS_1)) {
        Serial.println("[I2C] Bridge found!");
    } else {
        Serial.println("[I2C] Bridge not found - will keep trying");
    }
    
    // ========== WiFi Setup (optional) ==========
    setupWiFi();

    // ========== Min/Max Tracking initialisieren ==========
    Serial.println("\n[MinMax] Initializing tracking...");
    initMinMax(indoorMinMax);
    initMinMax(outdoorMinMax);

    // ========== Graph initialisieren ==========
    graphData.dataCount = 0;
    graphData.outdoorLoaded = false;
    graphData.indoorLoaded = false;
    lastModeChange = millis();  // Timer für Auto-Return

    // ========== InfluxDB initialisieren (wenn WiFi aktiv) ==========
    #ifdef ENABLE_INFLUXDB
    if (wifiConnected) {
        influxDBConnected = setupInfluxDB();
    }
    #endif

    // ========== Webserver starten (wenn WiFi aktiv) ==========
    if (wifiConnected) {
        setupWebServer();
    }

    // ========== Initial Display ==========
    drawIndoorSection();
    drawOutdoorSection();

    // WICHTIG: SD-Karte als LETZTES initialisieren!
    // LovyanGFX verwaltet Touch intern über Display-SPI
    // SD-Karte braucht separaten VSPI und muss NACH Display-Setup kommen
    delay(100);  // Kurze Pause damit Display/Touch komplett ready sind
    sdCardAvailable = initSDCard();

    Serial.println("\n[READY] System running!\n");
    if (sdCardAvailable) {
        Serial.println("[INFO] SD-Logging aktiv - Daten werden alle 15 Minuten gespeichert");
        Serial.println("[INFO] Monatliche CSV-Dateien (YYYYMM_indoor/outdoor.csv)");
    }
    if (wifiConnected) {
        Serial.println("[INFO] Webserver erreichbar unter: http://" + WiFi.localIP().toString());
    }
    #ifdef ENABLE_INFLUXDB
    if (influxDBConnected) {
        Serial.println("[INFO] InfluxDB aktiv - Daten werden parallel zu CSV gesendet");
    }
    #endif
}

// ==================== MAIN LOOP ====================

void loop() {
    unsigned long now = millis();

    // Touch prüfen (für Display-Modus-Wechsel)
    checkTouch();

    // Auto-Return nach 30 Sekunden prüfen
    checkAutoReturn();

    // I2C Daten abfragen
    if (now - lastI2CPoll >= I2C_POLL_INTERVAL) {
        lastI2CPoll = now;
        pollI2CData();
    }

    // Display aktualisieren - NUR im Normal-Modus (0)
    // Modi 1 (Min/Max) und 2 (Graph) werden nicht automatisch aktualisiert
    if (displayMode == 0 && now - lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL) {
        lastDisplayUpdate = now;

        drawHeader();

        if (indoorReceived) {
            drawIndoorSection();
        }

        if (outdoorReceived) {
            drawOutdoorSection();
        }
    }

    // SD-Karte: Daten loggen
    if (sdCardAvailable && now - lastSDLog >= SD_LOG_INTERVAL) {
        lastSDLog = now;

        if (indoorReceived) {
            logIndoorData();
        }

        if (outdoorReceived) {
            logOutdoorData();
        }

        // InfluxDB: Parallel zu SD-Karte senden
        #ifdef ENABLE_INFLUXDB
        if (influxDBConnected) {
            if (indoorReceived) {
                sendIndoorToInfluxDB();
            }
            if (outdoorReceived) {
                sendOutdoorToInfluxDB();
            }
        }
        #endif
    }

    // InfluxDB Reconnect (falls nicht verbunden aber WiFi aktiv)
    #ifdef ENABLE_INFLUXDB
    if (wifiConnected && !influxDBConnected && now - lastInfluxRetry >= INFLUX_RETRY_INTERVAL) {
        lastInfluxRetry = now;
        Serial.println("[InfluxDB] Retry connection...");
        influxDBConnected = setupInfluxDB();
    }
    #endif

    // WiFi Reconnect (falls nicht verbunden)
    if (!wifiConnected && now - lastWiFiRetry >= WIFI_RETRY_INTERVAL) {
        lastWiFiRetry = now;
        setupWiFi();
        if (wifiConnected) {
            setupWebServer();
            #ifdef ENABLE_INFLUXDB
            influxDBConnected = setupInfluxDB();
            #endif
        }
    }

    // Webserver verarbeiten
    if (wifiConnected) {
        server.handleClient();
    }

    delay(10);
}
