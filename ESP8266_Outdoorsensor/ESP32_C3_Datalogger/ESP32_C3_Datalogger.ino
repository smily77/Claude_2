/*
 * ESP32_C3_Datalogger.ino
 *
 * ESP32-C3 OLED 0.42" Datalogger für Indoor/Outdoor ESP-NOW Sensoren
 *
 * Hardware:
 * - ESP32-C3 mit 0.42" OLED Display (72x40 px)
 * - SD-Karte: MISO=7, CLK=6, MOSI=5, CS=4
 * - RGB LED (WS2812): Pin 3
 *
 * Features:
 * - Empfängt ESP-NOW Daten von Indoor/Outdoor Sensoren
 * - Speichert alle Daten als CSV auf SD-Karte
 * - OLED zeigt: Indoor/Outdoor Datensatz-Counter
 * - RGB LED Status:
 *   - BLAU:   Keine Daten von beiden Sensoren
 *   - AUS:    Normalbetrieb (Daten OK)
 *   - ROT:    Timeout erkannt
 */

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <SD.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_NeoPixel.h>

// ==================== KONFIGURATION ====================

// ESP-NOW
#define ESPNOW_CHANNEL 1

// SD-Karte Pins
#define SD_MISO 7
#define SD_CLK  6
#define SD_MOSI 5
#define SD_CS   4

// RGB LED (WS2812)
#define RGB_PIN 3
#define RGB_COUNT 1

// OLED Display (0.42" - 72x40 px)
#define SCREEN_WIDTH 72
#define SCREEN_HEIGHT 40
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

// CSV Dateien
#define INDOOR_CSV_FILE  "/indoor_log.csv"
#define OUTDOOR_CSV_FILE "/outdoor_log.csv"

// Timeout (2.1 × Sleep-Zeit, wie im Master)
#define TIMEOUT_MULTIPLIER 2.1

// ==================== DATENSTRUKTUREN ====================

// Indoor Sensor (mit Luftfeuchtigkeit)
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
    uint16_t sleep_time_sec;
} sensor_data_indoor;

// Outdoor Sensor (ohne Luftfeuchtigkeit)
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
    uint16_t sleep_time_sec;
} sensor_data_outdoor;

// ==================== GLOBALE VARIABLEN ====================

// Hardware
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_NeoPixel pixel(RGB_COUNT, RGB_PIN, NEO_GRB + NEO_KHZ800);

// Daten
sensor_data_indoor indoorData;
sensor_data_outdoor outdoorData;

// Status
bool indoorReceived = false;
bool outdoorReceived = false;
unsigned long lastIndoorTime = 0;
unsigned long lastOutdoorTime = 0;
uint32_t indoorCount = 0;
uint32_t outdoorCount = 0;

// SD-Karte
bool sdCardOK = false;

// RGB LED Farben
#define COLOR_OFF     pixel.Color(0, 0, 0)
#define COLOR_BLUE    pixel.Color(0, 0, 50)
#define COLOR_RED     pixel.Color(50, 0, 0)
#define COLOR_GREEN   pixel.Color(0, 50, 0)

// ==================== ESP-NOW CALLBACK ====================

void onDataRecv(const esp_now_recv_info* recv_info, const uint8_t *data, int data_len) {
    unsigned long now = millis();

    // Indoor oder Outdoor anhand der Größe erkennen
    bool isIndoor = (data_len >= sizeof(sensor_data_indoor));

    if (isIndoor) {
        // Indoor Daten
        memcpy(&indoorData, data, min((int)sizeof(indoorData), data_len));

        indoorReceived = true;
        lastIndoorTime = now;
        indoorCount++;

        // Auf SD-Karte speichern
        logIndoorData();

        Serial.println("\n=== Indoor Data ===");
        Serial.printf("Temp: %.1f°C, Hum: %.1f%%, Press: %.1f mbar\n",
                     indoorData.temperature, indoorData.humidity, indoorData.pressure);
        Serial.printf("Battery: %d mV, Count: %lu\n",
                     indoorData.battery_voltage, indoorCount);

    } else {
        // Outdoor Daten
        memcpy(&outdoorData, data, min((int)sizeof(outdoorData), data_len));

        outdoorReceived = true;
        lastOutdoorTime = now;
        outdoorCount++;

        // Auf SD-Karte speichern
        logOutdoorData();

        Serial.println("\n=== Outdoor Data ===");
        Serial.printf("Temp: %.1f°C, Press: %.1f mbar\n",
                     outdoorData.temperature, outdoorData.pressure);
        Serial.printf("Battery: %d mV, Count: %lu\n",
                     outdoorData.battery_voltage, outdoorCount);
    }

    // Display aktualisieren
    updateDisplay();

    // LED Status aktualisieren
    updateLED();
}

// ==================== SD-KARTE FUNKTIONEN ====================

bool initSDCard() {
    Serial.println("\n[SD] Initializing SD card...");

    SPI.begin(SD_CLK, SD_MISO, SD_MOSI, SD_CS);

    if (!SD.begin(SD_CS)) {
        Serial.println("[SD] Card Mount Failed!");
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
    Serial.printf("[SD] Size: %lluMB\n", cardSize);

    // CSV Header erstellen falls Dateien nicht existieren
    createCSVHeaders();

    return true;
}

void createCSVHeaders() {
    // Indoor CSV Header
    if (!SD.exists(INDOOR_CSV_FILE)) {
        File file = SD.open(INDOOR_CSV_FILE, FILE_WRITE);
        if (file) {
            file.println("Timestamp,Date,Time,Temperature,Humidity,Pressure,Battery_mV,Battery_Warning,RSSI,Sleep_Sec,Duration_ms,Error,Reset_Reason");
            file.close();
            Serial.println("[SD] Indoor CSV header created");
        }
    }

    // Outdoor CSV Header
    if (!SD.exists(OUTDOOR_CSV_FILE)) {
        File file = SD.open(OUTDOOR_CSV_FILE, FILE_WRITE);
        if (file) {
            file.println("Timestamp,Date,Time,Temperature,Pressure,Battery_mV,Battery_Warning,RSSI,Sleep_Sec,Duration_ms,Error,Reset_Reason");
            file.close();
            Serial.println("[SD] Outdoor CSV header created");
        }
    }
}

void logIndoorData() {
    if (!sdCardOK) return;

    File file = SD.open(INDOOR_CSV_FILE, FILE_APPEND);
    if (!file) {
        Serial.println("[SD] Failed to open indoor log");
        return;
    }

    // Timestamp (Unix-Zeit wäre besser, aber wir haben nur millis())
    unsigned long ts = millis();

    // CSV Zeile: Timestamp,Date,Time,Temp,Hum,Press,Batt,Warning,RSSI,Sleep,Duration,Error,Reset
    char line[256];
    snprintf(line, sizeof(line), "%lu,,,%.2f,%.2f,%.2f,%u,%u,,%u,%u,%u,%u",
             ts,
             indoorData.temperature,
             indoorData.humidity,
             indoorData.pressure,
             indoorData.battery_voltage,
             indoorData.battery_warning,
             indoorData.sleep_time_sec,
             indoorData.duration,
             indoorData.sensor_error,
             indoorData.reset_reason);

    file.println(line);
    file.close();

    Serial.printf("[SD] Indoor logged: %s\n", line);
}

void logOutdoorData() {
    if (!sdCardOK) return;

    File file = SD.open(OUTDOOR_CSV_FILE, FILE_APPEND);
    if (!file) {
        Serial.println("[SD] Failed to open outdoor log");
        return;
    }

    // Timestamp
    unsigned long ts = millis();

    // CSV Zeile: Timestamp,Date,Time,Temp,Press,Batt,Warning,RSSI,Sleep,Duration,Error,Reset
    char line[256];
    snprintf(line, sizeof(line), "%lu,,,%.2f,%.2f,%u,%u,,%u,%u,%u,%u",
             ts,
             outdoorData.temperature,
             outdoorData.pressure,
             outdoorData.battery_voltage,
             outdoorData.battery_warning,
             outdoorData.sleep_time_sec,
             outdoorData.duration,
             outdoorData.sensor_error,
             outdoorData.reset_reason);

    file.println(line);
    file.close();

    Serial.printf("[SD] Outdoor logged: %s\n", line);
}

// ==================== DISPLAY FUNKTIONEN ====================

void updateDisplay() {
    display.clearDisplay();

    // Kleine Schrift für 72x40 Display
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    // Zeile 1: Indoor Count
    display.setCursor(0, 0);
    display.print("IN:");
    display.print(indoorCount);

    // Zeile 2: Outdoor Count
    display.setCursor(0, 10);
    display.print("OUT:");
    display.print(outdoorCount);

    // Zeile 3: SD Status
    display.setCursor(0, 20);
    if (sdCardOK) {
        display.print("SD:OK");
    } else {
        display.print("SD:ERR");
    }

    // Zeile 4: Timeout Status
    display.setCursor(0, 30);
    if (!indoorReceived && !outdoorReceived) {
        display.print("WAIT");
    } else if (checkTimeout()) {
        display.print("TIMEOUT!");
    } else {
        display.print("OK");
    }

    display.display();
}

// ==================== LED FUNKTIONEN ====================

void updateLED() {
    // BLAU: Noch keine Daten von beiden
    if (!indoorReceived && !outdoorReceived) {
        pixel.setPixelColor(0, COLOR_BLUE);
        pixel.show();
        return;
    }

    // ROT: Timeout
    if (checkTimeout()) {
        pixel.setPixelColor(0, COLOR_RED);
        pixel.show();
        return;
    }

    // AUS: Normalbetrieb
    pixel.setPixelColor(0, COLOR_OFF);
    pixel.show();
}

// ==================== TIMEOUT-ERKENNUNG ====================

bool checkTimeout() {
    unsigned long now = millis();
    bool timeout = false;

    // Indoor Timeout prüfen
    if (indoorReceived) {
        unsigned long timeSince = (now - lastIndoorTime) / 1000; // Sekunden
        uint16_t timeoutSec = indoorData.sleep_time_sec * TIMEOUT_MULTIPLIER;
        if (timeSince > timeoutSec) {
            timeout = true;
        }
    }

    // Outdoor Timeout prüfen
    if (outdoorReceived) {
        unsigned long timeSince = (now - lastOutdoorTime) / 1000; // Sekunden
        uint16_t timeoutSec = outdoorData.sleep_time_sec * TIMEOUT_MULTIPLIER;
        if (timeSince > timeoutSec) {
            timeout = true;
        }
    }

    return timeout;
}

// ==================== SETUP ====================

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n\n========================================");
    Serial.println("   ESP32-C3 ESP-NOW Datalogger");
    Serial.println("========================================\n");

    // ========== RGB LED initialisieren ==========
    Serial.println("[LED] Initializing RGB LED...");
    pixel.begin();
    pixel.setBrightness(20); // Niedrige Helligkeit
    pixel.setPixelColor(0, COLOR_BLUE); // Startzustand: BLAU
    pixel.show();

    // ========== Display initialisieren ==========
    Serial.println("[OLED] Initializing display...");
    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println("[OLED] SSD1306 allocation failed!");
        while (1); // Halt
    }

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("Starting");
    display.println("Logger...");
    display.display();
    delay(1000);

    // ========== SD-Karte initialisieren ==========
    sdCardOK = initSDCard();

    if (!sdCardOK) {
        display.clearDisplay();
        display.setCursor(0, 0);
        display.println("SD CARD");
        display.println("ERROR!");
        display.display();
        pixel.setPixelColor(0, COLOR_RED);
        pixel.show();
        delay(3000);
    }

    // ========== WiFi & ESP-NOW initialisieren ==========
    Serial.println("\n[WiFi] Setting up ESP-NOW...");

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    // Kanal setzen
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(false);

    Serial.print("[WiFi] MAC: ");
    Serial.println(WiFi.macAddress());
    Serial.printf("[WiFi] Channel: %d\n", ESPNOW_CHANNEL);

    // ESP-NOW initialisieren
    if (esp_now_init() != ESP_OK) {
        Serial.println("[ESP-NOW] Init failed!");
        display.clearDisplay();
        display.setCursor(0, 0);
        display.println("ESP-NOW");
        display.println("ERROR!");
        display.display();
        while (1);
    }

    esp_now_register_recv_cb(onDataRecv);
    Serial.println("[ESP-NOW] Initialized");

    // ========== Bereit ==========
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("READY!");
    display.println("Waiting");
    display.println("for data");
    display.display();

    Serial.println("\n[READY] Datalogger running!\n");
    Serial.println("Waiting for ESP-NOW data...\n");
}

// ==================== MAIN LOOP ====================

void loop() {
    // Periodisch Display & LED aktualisieren
    static unsigned long lastUpdate = 0;

    if (millis() - lastUpdate >= 5000) { // Alle 5 Sekunden
        lastUpdate = millis();
        updateDisplay();
        updateLED();

        // Debug-Ausgabe
        Serial.printf("[Status] Indoor: %lu, Outdoor: %lu, SD: %s\n",
                     indoorCount, outdoorCount, sdCardOK ? "OK" : "ERROR");
    }

    delay(100);
}
