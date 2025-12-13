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
#define ENABLE_INFLUXDB

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

// ==================== GLOBALE VARIABLEN ====================

// Display
LGFX lcd;
LGFX_Sprite indoorSprite(&lcd);
LGFX_Sprite outdoorSprite(&lcd);
int screenWidth = 320;
int screenHeight = 240;
bool is480p = false;

// I2C Bridge
I2CSensorBridge i2cBridge;

// Daten
IndoorData indoorData;
OutdoorData outdoorData;
SystemStatus systemStatus;

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
            
            Serial.printf("[Indoor] Temp: %.1f°C, Hum: %.1f%%, Press: %.0f mbar\n",
                         indoorData.temperature, indoorData.humidity, indoorData.pressure);
        }
    }
    
    // Outdoor Daten (ID 0x02)
    if (newDataMask & 0x04) {  // Bit 2 für ID 0x02
        if (i2cBridge.readStruct(BRIDGE_ADDRESS_1, 0x02, outdoorData)) {
            outdoorReceived = true;
            lastOutdoorUpdate = millis();
            
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

    // SPI für SD-Karte initialisieren
    SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

    if (!SD.begin(SD_CS)) {
        Serial.println("[SD] No SD Card found or initialization failed");
        return false;
    }

    uint8_t cardType = SD.cardType();

    if (cardType == CARD_NONE) {
        Serial.println("[SD] No SD card attached");
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

    // ========== SD-Karte initialisieren ==========
    sdCardAvailable = initSDCard();

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

    // I2C Daten abfragen
    if (now - lastI2CPoll >= I2C_POLL_INTERVAL) {
        lastI2CPoll = now;
        pollI2CData();
    }

    // Display aktualisieren
    if (now - lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL) {
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
