/*
 * ESP32-C3 I2C Slave Test Program
 *
 * Hardware:
 * - ESP32-C3
 * - SDA: Pin 8
 * - SCL: Pin 9
 *
 * This program responds to I2C master requests
 */

#include <Wire.h>

// I2C Slave Address
#define SLAVE_ADDRESS 0x08

// I2C Pins for ESP32-C3
#define SDA_PIN 8
#define SCL_PIN 9

// Data storage
volatile int receivedData = 0;
volatile bool dataReceived = false;
int responseCounter = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n=== ESP32-C3 I2C Slave Test ===");
  Serial.printf("SDA: Pin %d, SCL: Pin %d\n", SDA_PIN, SCL_PIN);
  Serial.printf("Slave Address: 0x%02X\n", SLAVE_ADDRESS);

  // Initialize I2C as slave with custom pins
  Wire.begin(SLAVE_ADDRESS, SDA_PIN, SCL_PIN);

  // Register callbacks
  Wire.onReceive(receiveEvent);
  Wire.onRequest(requestEvent);

  Serial.println("I2C Slave initialized");
  Serial.println("Waiting for master...\n");
}

void loop() {
  // Check if data was received
  if (dataReceived) {
    dataReceived = false;
    Serial.printf("✓ Received from master: %d\n", receivedData);
  }

  delay(10); // Small delay to prevent busy-waiting
}

// Callback when data is received from master
void receiveEvent(int numBytes) {
  Serial.printf("← Master sends %d byte(s): ", numBytes);

  if (Wire.available()) {
    receivedData = Wire.read();
    Serial.println(receivedData);
    dataReceived = true;

    // Read any additional bytes
    while (Wire.available()) {
      byte b = Wire.read();
      Serial.printf("  Additional byte: %d\n", b);
    }
  }
}

// Callback when master requests data
void requestEvent() {
  responseCounter++;

  // Send 4 bytes back to master
  // We'll echo back the received data as a 32-bit integer
  int dataToSend = receivedData;

  byte response[4];
  response[0] = (dataToSend >> 24) & 0xFF;
  response[1] = (dataToSend >> 16) & 0xFF;
  response[2] = (dataToSend >> 8) & 0xFF;
  response[3] = dataToSend & 0xFF;

  Wire.write(response, 4);

  Serial.printf("→ Master requests data (#%d): Sending %d as 4 bytes: [%d, %d, %d, %d]\n",
                responseCounter, dataToSend,
                response[0], response[1], response[2], response[3]);
}
