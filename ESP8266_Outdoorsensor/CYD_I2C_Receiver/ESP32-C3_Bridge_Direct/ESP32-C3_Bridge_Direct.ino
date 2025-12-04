/*
 * ESP32-C3_Bridge_Direct.ino
 *
 * Bridge MIT ESP-NOW aber OHNE I2CSensorBridge Library
 * Verwendet direkt Wire.begin() wie das funktionierende Test-Programm
 *
 * Hardware:
 * - ESP32-C3
 * - I2C: GPIO 8 (SDA), GPIO 9 (SCL)
 */

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Wire.h>

// ==================== KONFIGURATION ====================

// ESP-NOW
#define ESPNOW_CHANNEL 1

// I2C
#define I2C_SLAVE_ADDRESS 0x08
#define I2C_SDA_PIN 6                 // GEÄNDERT von 8 zu 6 (WiFi-sicher!)
#define I2C_SCL_PIN 7                 // GEÄNDERT von 9 zu 7 (WiFi-sicher!)
#define I2C_BUFFER_SIZE 128

// ==================== DATENSTRUKTUREN ====================

struct IndoorData {
    float temperature;
    float humidity;
    float pressure;
    uint16_t battery_mv;
    uint32_t timestamp;
    int8_t rssi;
    bool battery_warning;
} __attribute__((packed));

struct OutdoorData {
    float temperature;
    float pressure;
    uint16_t battery_mv;
    uint32_t timestamp;
    int8_t rssi;
    bool battery_warning;
} __attribute__((packed));

// ESP-NOW Empfangs-Strukturen
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

IndoorData indoorData;
OutdoorData outdoorData;

unsigned long lastIndoorReceived = 0;
unsigned long lastOutdoorReceived = 0;
uint16_t totalPacketsReceived = 0;

volatile bool i2cDataRequested = false;
volatile uint8_t requestedDataType = 0; // 0=none, 1=indoor, 2=outdoor

// ==================== ESP-NOW CALLBACK ====================

void onDataReceive(const esp_now_recv_info* recv_info, const uint8_t* data, int data_len) {
    totalPacketsReceived++;

    bool isIndoor = (data_len >= sizeof(sensor_data_indoor_raw));

    if (isIndoor) {
        sensor_data_indoor_raw raw;
        memcpy(&raw, data, min(data_len, (int)sizeof(raw)));

        indoorData.temperature = raw.temperature;
        indoorData.humidity = raw.humidity;
        indoorData.pressure = raw.pressure;
        indoorData.battery_mv = raw.battery_voltage;
        indoorData.timestamp = millis();
        indoorData.rssi = recv_info->rx_ctrl->rssi;
        indoorData.battery_warning = raw.battery_warning;

        lastIndoorReceived = millis();

        Serial.printf("[INDOOR] Temp: %.1f°C, Hum: %.1f%%, Press: %.0f mbar\n",
                     indoorData.temperature, indoorData.humidity, indoorData.pressure);
    } else {
        sensor_data_outdoor_raw raw;
        memcpy(&raw, data, min(data_len, (int)sizeof(raw)));

        outdoorData.temperature = raw.temperature;
        outdoorData.pressure = raw.pressure;
        outdoorData.battery_mv = raw.battery_voltage;
        outdoorData.timestamp = millis();
        outdoorData.rssi = recv_info->rx_ctrl->rssi;
        outdoorData.battery_warning = raw.battery_warning;

        lastOutdoorReceived = millis();

        Serial.printf("[OUTDOOR] Temp: %.1f°C, Press: %.0f mbar\n",
                     outdoorData.temperature, outdoorData.pressure);
    }
}

// ==================== I2C CALLBACKS ====================

void onI2CReceive(int numBytes) {
    Serial.printf("[I2C RX] Received %d bytes: ", numBytes);

    if (numBytes >= 1) {
        uint8_t cmd = Wire.read();
        Serial.printf("CMD=0x%02X", cmd);

        requestedDataType = cmd; // 0x01=indoor, 0x02=outdoor
    }

    // Rest verwerfen
    while (Wire.available()) {
        Wire.read();
    }
    Serial.println();
}

void onI2CRequest() {
    Serial.printf("[I2C TX] Request - Type: %d\n", requestedDataType);

    if (requestedDataType == 0x01) {
        // Indoor Daten senden
        byte* dataPtr = (byte*)&indoorData;
        size_t written = Wire.write(dataPtr, sizeof(IndoorData));
        Serial.printf("  Sent Indoor: %d bytes\n", written);
    }
    else if (requestedDataType == 0x02) {
        // Outdoor Daten senden
        byte* dataPtr = (byte*)&outdoorData;
        size_t written = Wire.write(dataPtr, sizeof(OutdoorData));
        Serial.printf("  Sent Outdoor: %d bytes\n", written);
    }
    else {
        // Dummy-Daten
        byte dummy[4] = {0xDE, 0xAD, 0xBE, 0xEF};
        Wire.write(dummy, 4);
        Serial.println("  Sent dummy data");
    }
}

// ==================== SETUP ====================

void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial.println("\n===========================================");
    Serial.println("  ESP32-C3 Bridge DIRECT (No Library)");
    Serial.println("===========================================\n");

    // ========== WiFi & ESP-NOW initialisieren ==========
    Serial.println("[INIT] Setting up ESP-NOW...");

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(false);

    Serial.printf("[WiFi] Channel: %d\n", ESPNOW_CHANNEL);
    Serial.printf("[WiFi] MAC: %s\n", WiFi.macAddress().c_str());

    if (esp_now_init() != ESP_OK) {
        Serial.println("[ERROR] ESP-NOW init failed!");
        ESP.restart();
    }

    esp_now_register_recv_cb(onDataReceive);
    Serial.println("[ESP-NOW] ✓ Initialized");

    delay(500);

    // ========== I2C DIREKT initialisieren ==========
    Serial.println("\n[INIT] Setting up I2C (DIRECT Wire.begin)...");

    bool success = Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, I2C_SLAVE_ADDRESS);

    if (success) {
        Serial.println("[I2C] ✓ Wire.begin() succeeded");
    } else {
        Serial.println("[I2C] ✗ Wire.begin() failed!");
    }

    Wire.setBufferSize(I2C_BUFFER_SIZE);
    Serial.printf("[I2C] Buffer size: %d bytes\n", I2C_BUFFER_SIZE);

    Wire.onReceive(onI2CReceive);
    Wire.onRequest(onI2CRequest);
    Serial.println("[I2C] ✓ Callbacks registered");

    Serial.printf("[I2C] Slave Address: 0x%02X\n", I2C_SLAVE_ADDRESS);
    Serial.printf("[I2C] SDA: GPIO %d, SCL: GPIO %d\n", I2C_SDA_PIN, I2C_SCL_PIN);

    // Initial-Daten
    memset(&indoorData, 0, sizeof(indoorData));
    memset(&outdoorData, 0, sizeof(outdoorData));

    Serial.println("\n[READY] Bridge is running!");
    Serial.println("===========================================\n");
}

// ==================== MAIN LOOP ====================

void loop() {
    static unsigned long lastStatus = 0;

    if (millis() - lastStatus >= 10000) {
        lastStatus = millis();

        Serial.println("\n--- Status ---");
        Serial.printf("Packets: %d, Indoor: %lums ago, Outdoor: %lums ago\n",
                     totalPacketsReceived,
                     lastIndoorReceived > 0 ? (millis() - lastIndoorReceived) : 999999,
                     lastOutdoorReceived > 0 ? (millis() - lastOutdoorReceived) : 999999);
        Serial.println("--------------\n");
    }

    delay(10);
}
