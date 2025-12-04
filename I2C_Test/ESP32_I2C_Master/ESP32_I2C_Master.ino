/*
 * ESP32 I2C Master Test Program
 *
 * Hardware:
 * - ESP32 Dev Board
 * - SDA: Pin 22
 * - SCL: Pin 27
 *
 * This program sends data to an I2C slave and reads responses
 */

#include <Wire.h>

// I2C Slave Address
#define SLAVE_ADDRESS 0x08

// I2C Pins
#define SDA_PIN 22
#define SCL_PIN 27

// Timing
unsigned long lastSendTime = 0;
const unsigned long sendInterval = 2000; // Send every 2 seconds

int counter = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n=== ESP32 I2C Master Test ===");
  Serial.printf("SDA: Pin %d, SCL: Pin %d\n", SDA_PIN, SCL_PIN);
  Serial.printf("Slave Address: 0x%02X\n", SLAVE_ADDRESS);

  // Initialize I2C with custom pins
  Wire.begin(SDA_PIN, SCL_PIN);

  Serial.println("I2C Master initialized");
  Serial.println("Starting communication test...\n");
}

void loop() {
  unsigned long currentTime = millis();

  // Send data every 2 seconds
  if (currentTime - lastSendTime >= sendInterval) {
    lastSendTime = currentTime;
    counter++;

    // === WRITE TEST ===
    Serial.printf("--- Test #%d ---\n", counter);
    Serial.print("Sending to slave: ");
    Serial.println(counter);

    Wire.beginTransmission(SLAVE_ADDRESS);
    Wire.write(counter);
    byte error = Wire.endTransmission();

    if (error == 0) {
      Serial.println("✓ Send successful");
    } else {
      Serial.printf("✗ Send failed! Error: %d\n", error);
      printI2CError(error);
    }

    delay(100); // Short delay between write and read

    // === READ TEST ===
    Serial.print("Requesting data from slave... ");

    Wire.requestFrom(SLAVE_ADDRESS, 4); // Request 4 bytes

    if (Wire.available() >= 4) {
      byte response[4];
      for (int i = 0; i < 4; i++) {
        response[i] = Wire.read();
      }

      Serial.println("✓ Received:");
      Serial.printf("  Bytes: %d, %d, %d, %d\n",
                    response[0], response[1], response[2], response[3]);

      // Try to interpret as counter echo
      int receivedValue = (response[0] << 24) | (response[1] << 16) |
                          (response[2] << 8) | response[3];
      Serial.printf("  As int32: %d\n", receivedValue);
    } else {
      Serial.println("✗ No response from slave");
    }

    Serial.println();
  }
}

void printI2CError(byte error) {
  switch(error) {
    case 1:
      Serial.println("  (Data too long for transmit buffer)");
      break;
    case 2:
      Serial.println("  (NACK on transmit of address)");
      Serial.println("  → Slave not found! Check wiring and slave address.");
      break;
    case 3:
      Serial.println("  (NACK on transmit of data)");
      break;
    case 4:
      Serial.println("  (Other error)");
      break;
    default:
      Serial.println("  (Unknown error)");
  }
}
