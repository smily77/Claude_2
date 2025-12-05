/*
 * ESP32 I2C Scanner
 *
 * Hardware:
 * - ESP32 Dev Board
 * - SDA: Pin 22
 * - SCL: Pin 27
 *
 * This program scans the I2C bus to detect connected devices
 */

#include <Wire.h>

// I2C Pins
#define SDA_PIN 22
#define SCL_PIN 27

// I2C Clock Speed
#define I2C_FREQ 100000  // 100kHz

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n=== ESP32 I2C Scanner ===");
  Serial.printf("SDA: Pin %d, SCL: Pin %d\n", SDA_PIN, SCL_PIN);
  Serial.printf("I2C Frequency: %d Hz\n\n", I2C_FREQ);

  // Initialize I2C
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(I2C_FREQ);

  Serial.println("I2C Scanner initialized");
  Serial.println("Starting scan...\n");
}

void loop() {
  byte error, address;
  int deviceCount = 0;

  Serial.println("Scanning I2C bus (0x01 to 0x7F)...");
  Serial.println("------------------------------------");

  for (address = 1; address < 127; address++) {
    // Try to communicate with this address
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0) {
      // Device found!
      Serial.printf("✓ Device found at address 0x%02X", address);

      // Add common device names
      if (address == 0x08) {
        Serial.print(" (Expected ESP32-C3 Slave)");
      } else if (address >= 0x3C && address <= 0x3D) {
        Serial.print(" (Possible OLED Display)");
      } else if (address == 0x68 || address == 0x69) {
        Serial.print(" (Possible MPU6050 / RTC)");
      } else if (address >= 0x20 && address <= 0x27) {
        Serial.print(" (Possible PCF8574 I/O Expander)");
      } else if (address >= 0x48 && address <= 0x4F) {
        Serial.print(" (Possible ADS1115 / PCF8591)");
      }

      Serial.println();
      deviceCount++;
    } else if (error == 4) {
      // Unknown error - usually means device not present
      // Don't print anything for cleaner output
    } else if (error == 2) {
      // NACK on address - device not present
      // Don't print anything for cleaner output
    } else {
      // Other error
      Serial.printf("  Error %d at address 0x%02X\n", error, address);
    }
  }

  Serial.println("------------------------------------");

  if (deviceCount == 0) {
    Serial.println("✗ No I2C devices found!");
    Serial.println("\nTroubleshooting:");
    Serial.println("1. Check pull-up resistors (4.7kΩ on SDA and SCL)");
    Serial.println("2. Verify wiring connections");
    Serial.println("3. Ensure slave device is powered and running");
    Serial.println("4. Check that GND is connected between devices");
    Serial.println("5. For ESP32-C3: Verify I2C Slave mode is supported");
  } else {
    Serial.printf("✓ Found %d device(s) on I2C bus\n", deviceCount);

    // Check if expected slave is found
    Wire.beginTransmission(0x08);
    error = Wire.endTransmission();

    if (error == 0) {
      Serial.println("\n✓✓✓ ESP32-C3 Slave (0x08) is VISIBLE on the bus! ✓✓✓");
      Serial.println("    Hardware connection is OK!");
      Serial.println("    If Master program still fails, it's a software issue.");
    } else {
      Serial.println("\n✗✗✗ ESP32-C3 Slave (0x08) NOT found! ✗✗✗");
      Serial.println("    Possible causes:");
      Serial.println("    - ESP32-C3 Slave program not running");
      Serial.println("    - ESP32-C3 I2C Slave mode not working");
      Serial.println("    - Wrong I2C address configured");
    }
  }

  Serial.println("\nNext scan in 5 seconds...\n");
  delay(5000);
}
