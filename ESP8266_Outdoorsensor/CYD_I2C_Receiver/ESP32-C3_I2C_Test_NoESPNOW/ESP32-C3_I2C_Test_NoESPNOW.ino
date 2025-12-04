/*
 * ESP32-C3 I2C Slave - TEST VERSION (OHNE ESP-NOW)
 *
 * Simplified version to test if I2C works without WiFi/ESP-NOW interference
 *
 * Hardware:
 * - ESP32-C3
 * - SDA: Pin 8
 * - SCL: Pin 9
 */

#include <Wire.h>

// I2C Configuration
#define I2C_SLAVE_ADDRESS 0x20
#define I2C_SDA_PIN 8
#define I2C_SCL_PIN 9
#define I2C_BUFFER_SIZE 128

// Test data
struct TestData {
    float temperature;
    float humidity;
    uint16_t counter;
} __attribute__((packed));

TestData testData;
volatile bool newDataRequest = false;
volatile int requestCount = 0;
volatile int receiveCount = 0;

// I2C Callbacks
void onReceive(int numBytes) {
    receiveCount++;
    Serial.printf("\n[I2C RX] Received %d bytes (#%d)\n", numBytes, receiveCount);

    while (Wire.available()) {
        byte b = Wire.read();
        Serial.printf("  Data: 0x%02X\n", b);
    }
}

void onRequest() {
    requestCount++;

    // Send test data
    byte* dataPtr = (byte*)&testData;
    size_t dataSize = sizeof(testData);

    size_t written = Wire.write(dataPtr, min(dataSize, (size_t)32));

    Serial.printf("[I2C TX] Request #%d - Sent %d bytes (temp=%.1f, hum=%.1f, cnt=%d)\n",
                  requestCount, written,
                  testData.temperature, testData.humidity, testData.counter);
}

void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial.println("\n===========================================");
    Serial.println("  ESP32-C3 I2C Slave TEST (NO ESP-NOW)");
    Serial.println("===========================================\n");

    // Initialize test data
    testData.temperature = 22.5;
    testData.humidity = 55.0;
    testData.counter = 0;

    Serial.println("[INIT] Initializing I2C Slave...");
    Serial.printf("  Address: 0x%02X\n", I2C_SLAVE_ADDRESS);
    Serial.printf("  SDA: GPIO %d\n", I2C_SDA_PIN);
    Serial.printf("  SCL: GPIO %d\n", I2C_SCL_PIN);
    Serial.flush();

    // Initialize I2C as slave
    bool success = Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, I2C_SLAVE_ADDRESS);

    if (success) {
        Serial.println("  ✓ Wire.begin() succeeded");
    } else {
        Serial.println("  ✗ Wire.begin() failed!");
    }

    // Set buffer size
    Wire.setBufferSize(I2C_BUFFER_SIZE);
    Serial.printf("  ✓ Buffer size: %d bytes\n", I2C_BUFFER_SIZE);

    // Register callbacks
    Wire.onReceive(onReceive);
    Wire.onRequest(onRequest);
    Serial.println("  ✓ Callbacks registered");

    Serial.println("\n[READY] I2C Slave is running!");
    Serial.println("Waiting for I2C master requests...\n");
    Serial.println("Expected on scanner: Address 0x20 should be visible!");
    Serial.println("===========================================\n");
    Serial.flush();
}

void loop() {
    // Update test data every second
    static unsigned long lastUpdate = 0;

    if (millis() - lastUpdate >= 1000) {
        lastUpdate = millis();

        testData.counter++;
        testData.temperature += 0.1;
        testData.humidity -= 0.1;

        if (testData.temperature > 30.0) testData.temperature = 20.0;
        if (testData.humidity < 40.0) testData.humidity = 60.0;
    }

    // Status output every 10 seconds
    static unsigned long lastStatus = 0;
    if (millis() - lastStatus >= 10000) {
        lastStatus = millis();
        Serial.printf("[Status] Uptime: %lu s, Requests: %d, Receives: %d\n",
                      millis() / 1000, requestCount, receiveCount);
    }

    delay(10);
}
