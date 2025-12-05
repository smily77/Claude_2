/*
 * CYD_I2C_Master.ino
 * 
 * CYD Display als I2C Master
 * Liest Sensordaten von ESP32-C3 Bridge und zeigt sie an
 * Kann gleichzeitig WiFi für NTP nutzen
 * 
 * Hardware:
 * - CYD (ESP32-S3 mit Display)
 * - I2C: GPIO 43 (SDA), GPIO 44 (SCL) - sichere Pins beim N16R8!
 * - Pull-up Widerstände 4.7k an SDA/SCL
 */

#include <CYD_Display_Config.h>
#include <Credentials.h>
#include <WiFi.h>
#include <time.h>
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
} __attribute__((packed));

struct OutdoorData {
    float temperature;      // °C
    float pressure;        // mbar
    uint16_t battery_mv;   // mV
    uint32_t timestamp;    // ms
    int8_t rssi;          // dBm
    bool battery_warning;
} __attribute__((packed));

struct SystemStatus {
    uint8_t indoor_active;
    uint8_t outdoor_active;
    unsigned long indoor_last_seen;   // Sekunden
    unsigned long outdoor_last_seen;  // Sekunden
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
    
    // Titel
    lcd.setFont(is480p ? &fonts::FreeSansBold18pt7b : &fonts::FreeSansBold12pt7b);
    lcd.setTextColor(COLOR_TEXT);
    lcd.setTextDatum(top_center);
    //lcd.drawString("I2C Sensor Monitor", screenWidth / 2, is480p ? 10 : 5);
    
    // WiFi & Zeit Status (rechts oben)
    lcd.setFont(&fonts::Font2);
    lcd.setTextDatum(top_right);
    
    if (wifiConnected) {
        lcd.setTextColor(COLOR_RSSI_GOOD);
        lcd.drawString("WiFi", screenWidth - 5, 5);
        
        if (timeConfigured) {
            struct tm timeinfo;
            if (getLocalTime(&timeinfo)) {
                char timeStr[6];
                strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo);
                lcd.setTextColor(COLOR_TEXT);
                lcd.drawString(timeStr, screenWidth - 40, 5);
            }
        }
    } else {
        lcd.setTextColor(COLOR_RSSI_POOR);
        lcd.drawString("No WiFi", screenWidth - 5, 5);
    }
    
    // I2C Bridge Status (links oben)
    lcd.setTextDatum(top_left);
    if (systemStatus.esp_now_packets > 0) {
        lcd.setTextColor(COLOR_RSSI_GOOD);
        lcd.drawString("Bridge OK", 5, 5);
    } else {
        lcd.setTextColor(COLOR_RSSI_MEDIUM);
        lcd.drawString("Waiting...", 5, 5);
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
    
    if (!indoorReceived || !systemStatus.indoor_active) {
        indoorSprite.setFont(&fonts::FreeSans9pt7b);
        indoorSprite.setTextColor(COLOR_TEXT_DIM);
        indoorSprite.setTextDatum(middle_center);
        indoorSprite.drawString("Waiting for", spriteW / 2, spriteH / 2 - 15);
        indoorSprite.drawString("sensor data", spriteW / 2, spriteH / 2 + 10);
        indoorSprite.pushSprite(boxX + 2, spriteY);
        return;
    }
    
    int contentY = 15;
    int lineHeight = is480p ? 35 : 28;
    int centerX = spriteW / 2;
    
    // Temperatur
    indoorSprite.setFont(is480p ? &fonts::FreeSansBold24pt7b : &fonts::FreeSansBold18pt7b);
    indoorSprite.setTextColor(COLOR_TEMP);
    indoorSprite.setTextDatum(middle_center);
    char tempStr[16];
    snprintf(tempStr, sizeof(tempStr), "%.1f C", indoorData.temperature);
    indoorSprite.drawString(tempStr, centerX, contentY);
    contentY += lineHeight;
    
    // Luftfeuchtigkeit
    indoorSprite.setFont(is480p ? &fonts::FreeSansBold12pt7b : &fonts::FreeSans9pt7b);
    indoorSprite.setTextColor(COLOR_HUM);
    indoorSprite.drawString(String(indoorData.humidity, 0) + "%", centerX, contentY);
    contentY += lineHeight - 5;
    
    // Luftdruck
    indoorSprite.setTextColor(COLOR_PRESS);
    indoorSprite.drawString(String(indoorData.pressure, 0) + " mbar", centerX, contentY);
    contentY += lineHeight;
    
    // Batterie
    indoorSprite.setFont(&fonts::Font2);
    uint16_t battColor = indoorData.battery_warning ? COLOR_BATTERY_LOW : COLOR_BATTERY_OK;
    indoorSprite.setTextColor(battColor);
    indoorSprite.drawString(String(indoorData.battery_mv) + " mV", centerX, contentY);
    if (indoorData.battery_warning) {
        indoorSprite.setTextColor(COLOR_BATTERY_LOW);
        indoorSprite.drawString("LOW!", centerX, contentY + 18);
    }
    contentY += lineHeight - 12;
    
    // RSSI
    indoorSprite.setTextColor(getRSSIColor(indoorData.rssi));
    indoorSprite.drawString("RSSI: " + String(indoorData.rssi) + " dBm", centerX, contentY);
    contentY += lineHeight - 12;
    
    // Zeit seit letztem Update
    indoorSprite.setTextColor(COLOR_TEXT_DIM);
    indoorSprite.drawString(formatTime(systemStatus.indoor_last_seen), centerX, contentY);
    
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
    
    if (!outdoorReceived || !systemStatus.outdoor_active) {
        outdoorSprite.setFont(&fonts::FreeSans9pt7b);
        outdoorSprite.setTextColor(COLOR_TEXT_DIM);
        outdoorSprite.setTextDatum(middle_center);
        outdoorSprite.drawString("Waiting for", spriteW / 2, spriteH / 2 - 15);
        outdoorSprite.drawString("sensor data", spriteW / 2, spriteH / 2 + 10);
        outdoorSprite.pushSprite(boxX + 2, spriteY);
        return;
    }
    
    int contentY = 15;
    int lineHeight = is480p ? 35 : 28;
    int centerX = spriteW / 2;
    
    // Temperatur
    outdoorSprite.setFont(is480p ? &fonts::FreeSansBold24pt7b : &fonts::FreeSansBold18pt7b);
    outdoorSprite.setTextColor(COLOR_TEMP);
    outdoorSprite.setTextDatum(middle_center);
    char tempStr[16];
    snprintf(tempStr, sizeof(tempStr), "%.1f C", outdoorData.temperature);
    outdoorSprite.drawString(tempStr, centerX, contentY);
    contentY += lineHeight;
    
    // Keine Luftfeuchtigkeit
    outdoorSprite.setFont(is480p ? &fonts::FreeSansBold12pt7b : &fonts::FreeSans9pt7b);
    outdoorSprite.setTextColor(COLOR_TEXT_DIM);
    outdoorSprite.drawString("(no humidity)", centerX, contentY);
    contentY += lineHeight - 5;
    
    // Luftdruck
    outdoorSprite.setTextColor(COLOR_PRESS);
    outdoorSprite.drawString(String(outdoorData.pressure, 0) + " mbar", centerX, contentY);
    contentY += lineHeight;
    
    // Batterie
    outdoorSprite.setFont(&fonts::Font2);
    uint16_t battColor = outdoorData.battery_warning ? COLOR_BATTERY_LOW : COLOR_BATTERY_OK;
    outdoorSprite.setTextColor(battColor);
    outdoorSprite.drawString(String(outdoorData.battery_mv) + " mV", centerX, contentY);
    if (outdoorData.battery_warning) {
        outdoorSprite.setTextColor(COLOR_BATTERY_LOW);
        outdoorSprite.drawString("LOW!", centerX, contentY + 18);
    }
    contentY += lineHeight - 12;
    
    // RSSI
    outdoorSprite.setTextColor(getRSSIColor(outdoorData.rssi));
    outdoorSprite.drawString("RSSI: " + String(outdoorData.rssi) + " dBm", centerX, contentY);
    contentY += lineHeight - 12;
    
    // Zeit seit letztem Update
    outdoorSprite.setTextColor(COLOR_TEXT_DIM);
    outdoorSprite.drawString(formatTime(systemStatus.outdoor_last_seen), centerX, contentY);
    
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
            Serial.printf("[Status] Indoor: %s, Outdoor: %s, Packets: %d\n",
                         systemStatus.indoor_active ? "ON" : "OFF",
                         systemStatus.outdoor_active ? "ON" : "OFF",
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
    
    // ========== Initial Display ==========
    drawIndoorSection();
    drawOutdoorSection();
    
    Serial.println("\n[READY] System running!\n");
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
        
        if (indoorReceived || systemStatus.indoor_active) {
            drawIndoorSection();
        }
        
        if (outdoorReceived || systemStatus.outdoor_active) {
            drawOutdoorSection();
        }
    }
    
    // WiFi Reconnect (falls nicht verbunden)
    if (!wifiConnected && now - lastWiFiRetry >= WIFI_RETRY_INTERVAL) {
        lastWiFiRetry = now;
        setupWiFi();
    }
    
    delay(10);
}
