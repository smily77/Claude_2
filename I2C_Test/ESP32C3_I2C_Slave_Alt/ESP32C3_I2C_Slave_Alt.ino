/*
 * ESP32-C3 I2C Slave - Alternative Implementation
 *
 * Hardware:
 * - ESP32-C3
 * - SDA: Pin 8
 * - SCL: Pin 9
 *
 * This version tries different initialization methods for ESP32-C3
 *
 * IMPORTANT: Check Arduino ESP32 core version:
 * - Tools -> Board -> Boards Manager -> Search "esp32"
 * - Should be version 2.0.1 or newer for I2C slave support
 */

#include <Wire.h>

// I2C Slave Address
#define SLAVE_ADDRESS 0x08

// I2C Pins for ESP32-C3
#define SDA_PIN 8
#define SCL_PIN 9

// Data storage
volatile byte receivedData = 0;
volatile byte lastSentData = 0;
volatile int receiveCount = 0;
volatile int requestCount = 0;

void setup() {
  Serial.begin(115200);
  delay(2000);  // Longer delay to ensure serial is ready

  Serial.println("\n\n=== ESP32-C3 I2C Slave - Alternative ===");
  Serial.println("Testing different initialization methods...\n");

  // Print chip info
  Serial.print("Chip Model: ");
  Serial.println(ESP.getChipModel());
  Serial.print("Chip Revision: ");
  Serial.println(ESP.getChipRevision());
  Serial.print("SDK Version: ");
  Serial.println(ESP.getSdkVersion());
  Serial.println();

  // Try Method 1: Standard Wire.begin with address only
  Serial.println("Method 1: Wire.begin(SLAVE_ADDRESS)");
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
  Serial.println();

  // Print configuration
  Serial.println("Configuration:");
  Serial.printf("  Slave Address: 0x%02X (%d decimal)\n", SLAVE_ADDRESS, SLAVE_ADDRESS);
  Serial.printf("  SDA Pin: GPIO %d\n", SDA_PIN);
  Serial.printf("  SCL Pin: GPIO %d\n", SCL_PIN);
  Serial.println();

  Serial.println("✓ I2C Slave ready!");
  Serial.println("Waiting for master requests...\n");
  Serial.println("Expected behavior:");
  Serial.println("  1. Master writes data -> receiveEvent() called");
  Serial.println("  2. Master reads data -> requestEvent() called");
  Serial.println();
}

void loop() {
  // Keep the loop minimal - callbacks handle everything
  delay(10);

  // Optional: Print status every 10 seconds
  static unsigned long lastStatus = 0;
  if (millis() - lastStatus > 10000) {
    lastStatus = millis();
    Serial.printf("[Status] Received: %d times, Requested: %d times\n",
                  receiveCount, requestCount);
  }
}

// Callback when master writes data to slave
void receiveEvent(int numBytes) {
  receiveCount++;

  Serial.printf("\n[%lu] ← RECEIVE #%d: ", millis(), receiveCount);
  Serial.printf("%d byte(s) incoming\n", numBytes);

  // Read all bytes
  int idx = 0;
  while (Wire.available()) {
    byte b = Wire.read();

    if (idx == 0) {
      receivedData = b;  // Store first byte
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

  // Prepare response - send back the last received value
  byte response[4] = {0, 0, 0, receivedData};

  // Write to I2C buffer
  size_t written = Wire.write(response, 4);

  // Update last sent for debugging
  lastSentData = receivedData;

  Serial.printf("\n[%lu] → REQUEST #%d: ", millis(), requestCount);
  Serial.printf("Sent %d bytes: [%d, %d, %d, %d]\n",
                written, response[0], response[1], response[2], response[3]);
  Serial.printf("  Echoing value: %d\n", receivedData);

  if (written != 4) {
    Serial.printf("  ✗ WARNING: Only wrote %d of 4 bytes!\n", written);
  }
}
