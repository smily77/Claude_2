/*
 * ESP32-C3 I2C Slave - TEST MIT WiFi
 *
 * BEWEIS-TEST: Nimmt das funktionierende ESP32C3_I2C_Slave_Alt
 * und fügt NUR WiFi hinzu, um zu beweisen ob WiFi das Problem ist!
 *
 * Hardware:
 * - ESP32-C3
 * - SDA: Pin 8
 * - SCL: Pin 9
 */

#include <Wire.h>
#include <WiFi.h>
#include <esp_wifi.h>

// I2C Slave Address
#define SLAVE_ADDRESS 0x08

// I2C Pins for ESP32-C3
#define SDA_PIN 8
#define SCL_PIN 9

// WiFi
#define WIFI_CHANNEL 1

// Data storage
volatile byte receivedData = 0;
volatile byte lastSentData = 0;
volatile int receiveCount = 0;
volatile int requestCount = 0;

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println("\n\n===========================================");
  Serial.println("  ESP32-C3 I2C Slave - WIFI TEST");
  Serial.println("===========================================\n");
  Serial.println("BEWEIS: Funktioniert I2C noch MIT WiFi?\n");

  // ========== OPTION A: WiFi ZUERST ==========
  Serial.println("[TEST A] WiFi ZUERST, dann I2C...\n");

  Serial.println("[INIT] Setting up WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);
  Serial.printf("[WiFi] Channel: %d\n", WIFI_CHANNEL);
  Serial.printf("[WiFi] MAC: %s\n", WiFi.macAddress().c_str());
  Serial.println("[WiFi] ✓ Initialized");
  delay(500);

  // ========== I2C DANACH ==========
  Serial.println("\n[INIT] Setting up I2C (AFTER WiFi)...");
  Serial.printf("  Initializing as slave at address 0x%02X...\n", SLAVE_ADDRESS);

  if (Wire.begin((uint8_t)SLAVE_ADDRESS)) {
    Serial.println("  ✓ Wire.begin() returned true");
  } else {
    Serial.println("  ✗ Wire.begin() returned false");
  }

  // Register callbacks
  Wire.onReceive(receiveEvent);
  Wire.onRequest(requestEvent);
  Serial.println("  ✓ Callbacks registered");

  Serial.println("\nConfiguration:");
  Serial.printf("  Slave Address: 0x%02X\n", SLAVE_ADDRESS);
  Serial.printf("  SDA Pin: GPIO %d\n", SDA_PIN);
  Serial.printf("  SCL Pin: GPIO %d\n", SCL_PIN);

  Serial.println("\n✓ I2C Slave ready (WITH WiFi)!");
  Serial.println("===========================================\n");
  Serial.println("Jetzt Scanner laufen lassen!");
  Serial.println("Wenn Scanner 0x08 findet: WiFi ist NICHT das Problem");
  Serial.println("Wenn Scanner nichts findet: WiFi IST das Problem\n");
}

void loop() {
  delay(10);

  static unsigned long lastStatus = 0;
  if (millis() - lastStatus > 10000) {
    lastStatus = millis();
    Serial.printf("[Status] RX: %d, TX: %d\n", receiveCount, requestCount);
  }
}

// Callback when master writes data to slave
void receiveEvent(int numBytes) {
  receiveCount++;
  Serial.printf("\n[%lu] ← RECEIVE #%d: %d byte(s)\n", millis(), receiveCount, numBytes);

  int idx = 0;
  while (Wire.available()) {
    byte b = Wire.read();
    if (idx == 0) {
      receivedData = b;
      Serial.printf("  Data[0]: %d (0x%02X) <- STORED\n", b, b);
    } else {
      Serial.printf("  Data[%d]: %d (0x%02X)\n", idx, b, b);
    }
    idx++;
  }

  if (idx != numBytes) {
    Serial.printf("  ✗ WARNING: Expected %d bytes, got %d\n", numBytes, idx);
  }
}

// Callback when master requests data from slave
void requestEvent() {
  requestCount++;

  byte response[4] = {0, 0, 0, receivedData};
  size_t written = Wire.write(response, 4);
  lastSentData = receivedData;

  Serial.printf("\n[%lu] → REQUEST #%d: Sent %d bytes: [%d, %d, %d, %d]\n",
                millis(), requestCount, written,
                response[0], response[1], response[2], response[3]);
  Serial.printf("  Echoing value: %d\n", receivedData);

  if (written != 4) {
    Serial.printf("  ✗ WARNING: Only wrote %d of 4 bytes!\n", written);
  }
}
