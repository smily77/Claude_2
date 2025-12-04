/*
 * I2C Scanner für CYD (ESP32-S3)
 *
 * Scannt den I2C-Bus nach Geräten
 */

#include <Wire.h>

// I2C Pins (wie im CYD_I2C_Master)
#define SDA_PIN 22
#define SCL_PIN 27

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println("\n=== CYD I2C Scanner ===");
  Serial.printf("SDA: Pin %d, SCL: Pin %d\n\n", SDA_PIN, SCL_PIN);

  // Initialize I2C
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);  // 100kHz

  Serial.println("I2C Scanner initialized");
  Serial.println("Starting scan...\n");
}

void loop() {
  byte error, address;
  int deviceCount = 0;

  Serial.println("Scanning I2C bus (0x01 to 0x7F)...");
  Serial.println("------------------------------------");

  for (address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0) {
      Serial.printf("✓ Device found at address 0x%02X", address);

      if (address == 0x20) {
        Serial.print(" <- ESP32-C3 Bridge!");
      } else if (address == 0x08) {
        Serial.print(" <- Test Slave");
      }

      Serial.println();
      deviceCount++;
    }
  }

  Serial.println("------------------------------------");

  if (deviceCount == 0) {
    Serial.println("✗ No I2C devices found!");
    Serial.println("\nTroubleshooting:");
    Serial.println("1. Check wiring (SDA↔SDA, SCL↔SCL, GND↔GND)");
    Serial.println("2. Check pull-up resistors (4.7kΩ)");
    Serial.println("3. Ensure slave is powered and running");
    Serial.println("4. Check slave serial output");
  } else {
    Serial.printf("✓ Found %d device(s)\n", deviceCount);

    // Spezifisch nach Bridge suchen
    Wire.beginTransmission(0x20);
    error = Wire.endTransmission();

    if (error == 0) {
      Serial.println("\n✓✓✓ ESP32-C3 Bridge (0x20) is VISIBLE! ✓✓✓");
    } else {
      Serial.println("\n✗✗✗ ESP32-C3 Bridge (0x20) NOT found! ✗✗✗");
      Serial.println("    Check slave serial output!");
    }
  }

  Serial.println("\nNext scan in 5 seconds...\n");
  delay(5000);
}
