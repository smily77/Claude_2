/*
 * ESP32-C3_Bridge_Slave.ino
 * 
 * ESP32-C3 als Bridge zwischen ESP-NOW Sensoren und CYD Display
 * Empfängt Daten via ESP-NOW und stellt sie über I2C bereit
 * 
 * Hardware:
 * - ESP32-C3
 * - I2C: GPIO 8 (SDA), GPIO 9 (SCL)
 * - Pull-up Widerstände 4.7k an SDA/SCL
 */

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include "I2CSensorBridge.h"

// ==================== KONFIGURATION ====================

// ESP-NOW Konfiguration
#define ESPNOW_CHANNEL 1              // WiFi Kanal für ESP-NOW

// I2C Konfiguration
#define I2C_SLAVE_ADDRESS 0x20        // Diese Bridge hat Adresse 0x20
#define I2C_SDA_PIN 8                 // GPIO 8 für SDA
#define I2C_SCL_PIN 9                 // GPIO 9 für SCL

// Debug-Ausgaben
#define DEBUG_SERIAL 1                // Serielle Debug-Ausgaben

// ==================== DATENSTRUKTUREN ====================

// Indoor Sensor Daten (mit Luftfeuchtigkeit)
struct IndoorData {
    float temperature;      // °C
    float humidity;        // %
    float pressure;        // mbar
    uint16_t battery_mv;   // mV
    uint32_t timestamp;    // ms
    int8_t rssi;          // dBm
    bool battery_warning;
} __attribute__((packed));

// Outdoor Sensor Daten (ohne Luftfeuchtigkeit)  
struct OutdoorData {
    float temperature;      // °C
    float pressure;        // mbar
    uint16_t battery_mv;   // mV
    uint32_t timestamp;    // ms
    int8_t rssi;          // dBm
    bool battery_warning;
} __attribute__((packed));

// System Status (optional)
struct SystemStatus {
    uint8_t indoor_active;
    uint8_t outdoor_active;
    unsigned long indoor_last_seen;   // ms seit letztem Empfang
    unsigned long outdoor_last_seen;  // ms seit letztem Empfang
    uint16_t esp_now_packets;         // Anzahl empfangener Pakete
    uint8_t wifi_channel;
} __attribute__((packed));

// ==================== ESP-NOW EMPFANGS-STRUKTUREN ====================

// Original Sensor-Struktur (wie von ESP8266 gesendet)
typedef struct sensor_data_indoor_raw {
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
} sensor_data_indoor_raw;

typedef struct sensor_data_outdoor_raw {
    uint32_t timestamp;
    float temperature;
    float pressure;
    uint16_t battery_voltage;
    uint16_t duration;
    uint8_t battery_warning;
    uint8_t sensor_error;
    uint8_t reset_reason;
    uint8_t sensor_type;
} sensor_data_outdoor_raw;

// ==================== GLOBALE VARIABLEN ====================

// I2C Bridge
I2CSensorBridge i2cBridge;

// Daten-Instanzen
IndoorData indoorData;
OutdoorData outdoorData;
SystemStatus systemStatus;

// Timing
unsigned long lastIndoorReceived = 0;
unsigned long lastOutdoorReceived = 0;
uint16_t totalPacketsReceived = 0;

// ==================== ESP-NOW CALLBACK ====================

void onDataReceive(const esp_now_recv_info* recv_info, const uint8_t* data, int data_len) {
    // MAC-Adresse des Senders für Debug
    #if DEBUG_SERIAL
    Serial.printf("\n[ESP-NOW] Data received from %02X:%02X:%02X:%02X:%02X:%02X\n",
                  recv_info->src_addr[0], recv_info->src_addr[1], 
                  recv_info->src_addr[2], recv_info->src_addr[3],
                  recv_info->src_addr[4], recv_info->src_addr[5]);
    Serial.printf("          Size: %d bytes, RSSI: %d dBm\n", 
                  data_len, recv_info->rx_ctrl->rssi);
    #endif
    
    totalPacketsReceived++;
    
    // Indoor oder Outdoor anhand der Größe erkennen
    bool isIndoor = (data_len >= sizeof(sensor_data_indoor_raw));
    
    if (isIndoor) {
        // Indoor Daten verarbeiten
        sensor_data_indoor_raw raw;
        memcpy(&raw, data, min(data_len, (int)sizeof(raw)));
        
        // In Bridge-Format konvertieren
        indoorData.temperature = raw.temperature;
        indoorData.humidity = raw.humidity;
        indoorData.pressure = raw.pressure;
        indoorData.battery_mv = raw.battery_voltage;
        indoorData.timestamp = millis();
        indoorData.rssi = recv_info->rx_ctrl->rssi;
        indoorData.battery_warning = raw.battery_warning;
        
        // Via I2C Bridge aktualisieren
        i2cBridge.updateStruct(0x01, indoorData);
        
        lastIndoorReceived = millis();
        systemStatus.indoor_active = 1;
        
        #if DEBUG_SERIAL
        Serial.printf("[INDOOR]  Temp: %.1f°C, Hum: %.1f%%, Press: %.0f mbar, Batt: %d mV\n",
                     indoorData.temperature, indoorData.humidity, 
                     indoorData.pressure, indoorData.battery_mv);
        #endif
        
    } else {
        // Outdoor Daten verarbeiten
        sensor_data_outdoor_raw raw;
        memcpy(&raw, data, min(data_len, (int)sizeof(raw)));
        
        // In Bridge-Format konvertieren
        outdoorData.temperature = raw.temperature;
        outdoorData.pressure = raw.pressure;
        outdoorData.battery_mv = raw.battery_voltage;
        outdoorData.timestamp = millis();
        outdoorData.rssi = recv_info->rx_ctrl->rssi;
        outdoorData.battery_warning = raw.battery_warning;
        
        // Via I2C Bridge aktualisieren
        i2cBridge.updateStruct(0x02, outdoorData);
        
        lastOutdoorReceived = millis();
        systemStatus.outdoor_active = 1;
        
        #if DEBUG_SERIAL
        Serial.printf("[OUTDOOR] Temp: %.1f°C, Press: %.0f mbar, Batt: %d mV\n",
                     outdoorData.temperature, outdoorData.pressure, 
                     outdoorData.battery_mv);
        #endif
    }
    
    // System Status aktualisieren
    updateSystemStatus();
}

// ==================== HILFSFUNKTIONEN ====================

void updateSystemStatus() {
    unsigned long now = millis();
    
    // Zeit seit letztem Empfang berechnen
    systemStatus.indoor_last_seen = (lastIndoorReceived > 0) ? 
                                    (now - lastIndoorReceived) / 1000 : 999999;
    systemStatus.outdoor_last_seen = (lastOutdoorReceived > 0) ? 
                                     (now - lastOutdoorReceived) / 1000 : 999999;
    
    // Aktiv-Status (wenn länger als 5 Minuten nichts empfangen)
    if (systemStatus.indoor_last_seen > 300) {
        systemStatus.indoor_active = 0;
    }
    if (systemStatus.outdoor_last_seen > 300) {
        systemStatus.outdoor_active = 0;
    }
    
    systemStatus.esp_now_packets = totalPacketsReceived;
    systemStatus.wifi_channel = ESPNOW_CHANNEL;
    
    // Status via I2C aktualisieren
    i2cBridge.updateStruct(0x03, systemStatus);
}

// ==================== SETUP ====================

void setup() {
    Serial.begin(115200);
    delay(2000);  // Längere Wartezeit für Serial

    Serial.println("\n\n===========================================");
    Serial.println("     ESP32-C3 I2C Bridge for ESP-NOW");
    Serial.println("===========================================\n");
    Serial.flush();  // Sicherstellen dass alles gesendet wird

    // ========== WiFi & ESP-NOW ZUERST initialisieren ==========
    // WICHTIG: WiFi/ESP-NOW vor I2C, da WiFi.mode() Pin-Konfiguration ändern könnte!
    Serial.println("[INIT] Setting up ESP-NOW...");
    Serial.flush();

    // WiFi im Station Mode ohne Verbindung
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    // Kanal setzen
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(false);

    Serial.printf("[WiFi] Channel: %d\n", ESPNOW_CHANNEL);
    Serial.print("[WiFi] MAC Address: ");
    Serial.println(WiFi.macAddress());

    // ESP-NOW initialisieren
    if (esp_now_init() != ESP_OK) {
        Serial.println("[ERROR] ESP-NOW init failed!");
        ESP.restart();
    }

    // Callback registrieren
    esp_now_register_recv_cb(onDataReceive);

    Serial.println("[ESP-NOW] Initialized and listening");
    Serial.flush();
    delay(500);  // Wichtig: WiFi stabilisieren lassen

    // ========== I2C Bridge NACH WiFi initialisieren ==========
    Serial.println("\n[INIT] Setting up I2C Bridge...");
    Serial.flush();
    delay(100);

    // Als I2C Slave initialisieren
    i2cBridge.beginSlave(I2C_SLAVE_ADDRESS, I2C_SDA_PIN, I2C_SCL_PIN);

    delay(100);
    Serial.println("[I2C]  beginSlave() completed");
    Serial.flush();

    // Structs registrieren
    i2cBridge.registerStruct(0x01, &indoorData, 1, "Indoor");
    i2cBridge.registerStruct(0x02, &outdoorData, 1, "Outdoor");
    i2cBridge.registerStruct(0x03, &systemStatus, 1, "Status");

    Serial.printf("[I2C]  Slave Address: 0x%02X\n", I2C_SLAVE_ADDRESS);
    Serial.printf("[I2C]  SDA: GPIO %d, SCL: GPIO %d\n", I2C_SDA_PIN, I2C_SCL_PIN);
    Serial.println("[I2C]  Registered 3 data structures");
    Serial.flush();
    delay(100);
    
    // ========== Initial-Daten ==========
    
    // Leere Daten initialisieren
    memset(&indoorData, 0, sizeof(indoorData));
    memset(&outdoorData, 0, sizeof(outdoorData));
    memset(&systemStatus, 0, sizeof(systemStatus));
    
    systemStatus.wifi_channel = ESPNOW_CHANNEL;
    
    Serial.println("\n[READY] Bridge is running!");
    Serial.println("========================================\n");
    Serial.flush();

    // Info-Ausgabe
    delay(500);
    Serial.println("Waiting for sensor data...");
    Serial.println("Indoor sensor should send every 60 seconds");
    Serial.println("Outdoor sensor should send every 15 minutes\n");
    Serial.flush();
}

// ==================== MAIN LOOP ====================

void loop() {
    // System Status periodisch aktualisieren
    static unsigned long lastStatusUpdate = 0;
    
    if (millis() - lastStatusUpdate >= 5000) {  // Alle 5 Sekunden
        lastStatusUpdate = millis();
        updateSystemStatus();
        
        #if DEBUG_SERIAL
        // Status-Report
        Serial.println("\n--- Status Report ---");
        Serial.printf("Indoor:  %s (last: %lu sec ago)\n", 
                     systemStatus.indoor_active ? "ACTIVE" : "INACTIVE",
                     systemStatus.indoor_last_seen);
        Serial.printf("Outdoor: %s (last: %lu sec ago)\n",
                     systemStatus.outdoor_active ? "ACTIVE" : "INACTIVE", 
                     systemStatus.outdoor_last_seen);
        Serial.printf("Total packets: %d\n", systemStatus.esp_now_packets);
        
        // I2C Status
        uint8_t i2cStatus = i2cBridge.getStatusByte();
        Serial.printf("I2C new data flags: 0x%02X\n", i2cStatus);
        Serial.println("---------------------\n");
        #endif
    }
    
    // Kleine Pause für stabilen Betrieb
    delay(10);
}
