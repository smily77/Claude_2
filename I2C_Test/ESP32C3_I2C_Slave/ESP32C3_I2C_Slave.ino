/*
 * ESP32-C3 I2C Slave Test Program
 *
 * Hardware:
 * - ESP32-C3
 * - SDA: Pin 8 (default)
 * - SCL: Pin 9 (default)
 *
 * This program responds to I2C master requests
 *
 * NOTE: ESP32-C3 I2C Slave support may vary depending on the Wire library version
 */

#include <Wire.h>

// I2C Slave Address
#define SLAVE_ADDRESS 0x08

// I2C Pins for ESP32-C3 (using default I2C pins)
#define SDA_PIN 8
#define SCL_PIN 9

// I2C buffer size
#define I2C_BUFFER_SIZE 32

// Data storage
volatile byte receivedData = 0;
volatile bool dataReceived = false;
volatile bool requestPending = false;
int responseCounter = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n=== ESP32-C3 I2C Slave Test ===");
  Serial.printf("SDA: Pin %d, SCL: Pin %d\n", SDA_PIN, SCL_PIN);
  Serial.printf("Slave Address: 0x%02X\n", SLAVE_ADDRESS);
  Serial.printf("I2C Buffer Size: %d bytes\n", I2C_BUFFER_SIZE);

  // Initialize I2C as slave
  // ESP32-C3 syntax: Wire.begin(sda, scl, address)
  bool success = Wire.begin(SDA_PIN, SCL_PIN, SLAVE_ADDRESS);

  if (!success) {
    Serial.println("✗ I2C Slave initialization FAILED!");
    Serial.println("  Trying alternative initialization...");
    // Alternative method
    Wire.begin(SLAVE_ADDRESS);
  }

  // Set buffer size (important for ESP32-C3)
  Wire.setBufferSize(I2C_BUFFER_SIZE);

  // Register callbacks AFTER Wire.begin
  Wire.onReceive(receiveEvent);
  Wire.onRequest(requestEvent);

  Serial.println("✓ I2C Slave initialized");
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

  // Read all available bytes
  int bytesRead = 0;
  while (Wire.available() && bytesRead < numBytes) {
    byte b = Wire.read();
    if (bytesRead == 0) {
      receivedData = b;  // Store first byte
      Serial.print(b);
    } else {
      Serial.printf(", %d", b);
    }
    bytesRead++;
  }
  Serial.println();

  if (bytesRead > 0) {
    dataReceived = true;
  }

  Serial.printf("  Stored value: %d\n", receivedData);
}

// Callback when master requests data
void requestEvent() {
  responseCounter++;

  // Prepare 4 bytes to send back (echo the received data)
  byte response[4];
  response[0] = 0;               // MSB
  response[1] = 0;
  response[2] = 0;
  response[3] = receivedData;    // LSB - the actual data

  // Write data to I2C buffer
  size_t written = Wire.write(response, 4);

  Serial.printf("→ Master requests data (#%d): Sent %d bytes [%d, %d, %d, %d] (value: %d)\n",
                responseCounter, written,
                response[0], response[1], response[2], response[3],
                receivedData);

  if (written != 4) {
    Serial.printf("  ✗ WARNING: Only %d bytes written instead of 4!\n", written);
  }
}
