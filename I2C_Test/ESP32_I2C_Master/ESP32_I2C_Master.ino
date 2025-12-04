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

// I2C Clock Speed (try slower speed if problems occur)
#define I2C_FREQ 100000  // 100kHz (standard mode)
// #define I2C_FREQ 400000  // 400kHz (fast mode) - uncomment if 100kHz works

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
  Serial.printf("I2C Frequency: %d Hz\n", I2C_FREQ);

  // Initialize I2C with custom pins
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(I2C_FREQ);

  Serial.println("✓ I2C Master initialized");
  Serial.println("Starting communication test...\n");

  delay(500); // Give slave time to initialize
}

void loop() {
  unsigned long currentTime = millis();

  // Send data every 2 seconds
  if (currentTime - lastSendTime >= sendInterval) {
    lastSendTime = currentTime;
    counter++;

    // === WRITE TEST ===
    Serial.printf("--- Test #%d ---\n", counter);
    Serial.printf("Sending to slave (0x%02X): %d\n", SLAVE_ADDRESS, counter % 256);

    Wire.beginTransmission(SLAVE_ADDRESS);
    Wire.write((byte)(counter % 256));  // Send as single byte
    byte error = Wire.endTransmission();

    if (error == 0) {
      Serial.println("✓ Write successful");
    } else {
      Serial.printf("✗ Write failed! Error: %d\n", error);
      printI2CError(error);
      Serial.println();
      return; // Skip read if write failed
    }

    delay(250); // Longer delay between write and read (give slave time to prepare)

    // === READ TEST ===
    Serial.print("Requesting 4 bytes from slave... ");

    uint8_t bytesReceived = Wire.requestFrom((uint8_t)SLAVE_ADDRESS, (uint8_t)4);

    if (bytesReceived > 0) {
      Serial.printf("Got %d byte(s)\n", bytesReceived);

      byte response[4] = {0, 0, 0, 0};
      int idx = 0;

      // Read with timeout
      unsigned long startTime = millis();
      while (Wire.available() && idx < 4 && (millis() - startTime < 1000)) {
        response[idx++] = Wire.read();
      }

      if (idx > 0) {
        Serial.println("✓ Received:");
        Serial.printf("  Bytes: %d, %d, %d, %d\n",
                      response[0], response[1], response[2], response[3]);

        // Interpret as little-endian int32
        int receivedValue = response[3];  // Our slave sends value in LSB
        Serial.printf("  Echoed value: %d", receivedValue);

        if (receivedValue == (counter % 256)) {
          Serial.println(" ✓ MATCH!");
        } else {
          Serial.printf(" ✗ (Expected %d)\n", counter % 256);
        }
      } else {
        Serial.println("✗ Timeout waiting for data");
      }
    } else {
      Serial.println("✗ FAILED");
      Serial.println("  Slave did not acknowledge request");
      Serial.println("  Check:");
      Serial.println("  - Slave is running and initialized");
      Serial.println("  - Wire.onRequest() callback is registered");
      Serial.println("  - Pull-up resistors are present");
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
