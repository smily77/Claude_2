/*
 * ESP-NOW Empfänger für ESP32-C3
 * Empfängt Daten von Indoor- und Outdoor-Sensoren
 *
 * Hardware: ESP32-C3 (oder andere ESP32 Varianten)
 *
 * Unterstützt:
 * - Outdoor-Sensor (nur BMP180)
 * - Indoor-Sensor (BMP180 + AM2321 mit Luftfeuchtigkeit)
 *
 * HINWEIS: ESP32 verwendet eine andere ESP-NOW API als ESP8266!
 */

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

// ==================== KONFIGURATION ====================

// WiFi Kanal (MUSS mit Sender übereinstimmen!)
#define ESPNOW_CHANNEL 1

// ==================== DATENSTRUKTUR ====================
// Universal-Struktur für Indoor und Outdoor

typedef struct sensor_data_indoor {
  uint32_t timestamp;
  float temperature;
  float pressure;
  float humidity;           // Nur bei Indoor
  uint8_t am2321_readings;  // Nur bei Indoor
  uint16_t battery_voltage;
  uint16_t duration;
  uint8_t battery_warning;
  uint8_t sensor_error;
  uint8_t reset_reason;
  uint8_t sensor_type;      // 0=Outdoor, 1=Indoor
  uint16_t sleep_time_sec;  // Sleep-Periode in Sekunden
} sensor_data_indoor;

typedef struct sensor_data_outdoor {
  uint32_t timestamp;
  float temperature;
  float pressure;
  uint16_t battery_voltage;
  uint16_t duration;
  uint8_t battery_warning;
  uint8_t sensor_error;
  uint8_t reset_reason;
  uint8_t sensor_type;      // 0=Outdoor, 1=Indoor
  uint16_t sleep_time_sec;  // Sleep-Periode in Sekunden
} sensor_data_outdoor;

sensor_data_indoor dataIndoor;
sensor_data_outdoor dataOutdoor;
unsigned long lastReceiveTime = 0;
int receivedPackets = 0;

// ==================== FUNKTIONEN ====================

// ESP-NOW Receive Callback (ESP32 Version - neue API)
void onDataRecv(const esp_now_recv_info* recv_info, const uint8_t *data, int data_len) {
  receivedPackets++;

  Serial.println("\n========================================");
  Serial.printf("Data Received! (Packet #%d)\n", receivedPackets);
  Serial.println("========================================");

  // Sender MAC anzeigen (aus recv_info)
  Serial.print("From MAC: ");
  for (int i = 0; i < 6; i++) {
    Serial.printf("%02X", recv_info->src_addr[i]);
    if (i < 5) Serial.print(":");
  }
  Serial.println();

  // Sensor-Typ erkennen anhand Datengröße
  // Outdoor: 20 bytes, Indoor: 25 bytes
  bool isIndoor = (data_len >= sizeof(sensor_data_indoor));

  Serial.print("Data Size: ");
  Serial.print(data_len);
  Serial.println(" bytes");

  Serial.print("Sensor Type: ");
  if (isIndoor) {
    Serial.println("INDOOR (BMP180 + AM2321)");
    memcpy(&dataIndoor, data, min((int)sizeof(dataIndoor), data_len));

    // Daten anzeigen
    Serial.println("\n--- Sensor Data (Indoor) ---");
    Serial.print("Timestamp: ");
    Serial.print(dataIndoor.timestamp);
    Serial.println(" ms");

    Serial.print("Temperature: ");
    Serial.print(dataIndoor.temperature, 2);
    Serial.println(" °C");

    Serial.print("Humidity: ");
    Serial.print(dataIndoor.humidity, 1);
    Serial.println(" %");

    Serial.print("Pressure: ");
    Serial.print(dataIndoor.pressure, 2);
    Serial.println(" mbar");

    // Höhe über NN schätzen
    float altitude = 44330.0 * (1.0 - pow(dataIndoor.pressure / 1013.25, 0.1903));
    Serial.print("Estimated Altitude: ");
    Serial.print(altitude, 1);
    Serial.println(" m");

    Serial.print("AM2321 Readings: ");
    Serial.println(dataIndoor.am2321_readings);

    Serial.print("Battery: ");
    Serial.print(dataIndoor.battery_voltage);
    Serial.print(" mV (");
    Serial.print(dataIndoor.battery_voltage / 1000.0, 2);
    Serial.print(" V)");
    if (dataIndoor.battery_warning) {
      Serial.print(" ⚠️  [LOW BATTERY!]");
    }
    Serial.println();

    Serial.print("Duration: ");
    Serial.print(dataIndoor.duration);
    Serial.println(" ms");

    Serial.print("Sensor Status: ");
    if (dataIndoor.sensor_error == 0) {
      Serial.println("OK ✓");
    } else {
      Serial.printf("ERROR (Code: %d)\n", dataIndoor.sensor_error);
    }

    Serial.print("Reset Reason: ");
    switch (dataIndoor.reset_reason) {
      case 0: Serial.println("Power-on"); break;
      case 1: Serial.println("Hardware Watchdog"); break;
      case 2: Serial.println("Software Watchdog"); break;
      case 3: Serial.println("Software Reset"); break;
      case 4: Serial.println("Deep Sleep Wake"); break;
      case 5: Serial.println("External Reset"); break;
      default: Serial.println("Unknown");
    }

    Serial.print("Sleep Time: ");
    Serial.print(dataIndoor.sleep_time_sec);
    Serial.println(" sec");

  } else {
    Serial.println("OUTDOOR (BMP180 only)");
    memcpy(&dataOutdoor, data, min((int)sizeof(dataOutdoor), data_len));

    // Daten anzeigen
    Serial.println("\n--- Sensor Data (Outdoor) ---");
    Serial.print("Timestamp: ");
    Serial.print(dataOutdoor.timestamp);
    Serial.println(" ms");

    Serial.print("Temperature: ");
    Serial.print(dataOutdoor.temperature, 2);
    Serial.println(" °C");

    Serial.print("Pressure: ");
    Serial.print(dataOutdoor.pressure, 2);
    Serial.println(" mbar");

    // Höhe über NN schätzen
    float altitude = 44330.0 * (1.0 - pow(dataOutdoor.pressure / 1013.25, 0.1903));
    Serial.print("Estimated Altitude: ");
    Serial.print(altitude, 1);
    Serial.println(" m");

    Serial.print("Battery: ");
    Serial.print(dataOutdoor.battery_voltage);
    Serial.print(" mV (");
    Serial.print(dataOutdoor.battery_voltage / 1000.0, 2);
    Serial.print(" V)");
    if (dataOutdoor.battery_warning) {
      Serial.print(" ⚠️  [LOW BATTERY!]");
    }
    Serial.println();

    Serial.print("Duration: ");
    Serial.print(dataOutdoor.duration);
    Serial.println(" ms");

    Serial.print("Sensor Status: ");
    if (dataOutdoor.sensor_error == 0) {
      Serial.println("OK ✓");
    } else {
      Serial.printf("ERROR (Code: %d)\n", dataOutdoor.sensor_error);
    }

    Serial.print("Reset Reason: ");
    switch (dataOutdoor.reset_reason) {
      case 0: Serial.println("Power-on"); break;
      case 1: Serial.println("Hardware Watchdog"); break;
      case 2: Serial.println("Software Watchdog"); break;
      case 3: Serial.println("Software Reset"); break;
      case 4: Serial.println("Deep Sleep Wake"); break;
      case 5: Serial.println("External Reset"); break;
      default: Serial.println("Unknown");
    }

    Serial.print("Sleep Time: ");
    Serial.print(dataOutdoor.sleep_time_sec);
    Serial.println(" sec");
  }

  // RSSI anzeigen (Signalstärke - aus recv_info)
  Serial.print("Signal Strength (RSSI): ");
  Serial.print(recv_info->rx_ctrl->rssi);
  Serial.println(" dBm");

  Serial.println("========================================\n");

  lastReceiveTime = millis();

  // ==================== DATENWEITERVERARBEITUNG ====================
  // Hier können Sie die Daten weiterverarbeiten:

  // Beispiel 1: An MQTT senden
  // publishToMQTT(receivedData);

  // Beispiel 2: Auf Display anzeigen
  // updateDisplay(receivedData);

  // Beispiel 3: In Datenbank speichern
  // saveToDatabase(receivedData);

  // Beispiel 4: Bedingung prüfen (Alarm bei hoher Temperatur)
  float temp = isIndoor ? dataIndoor.temperature : dataOutdoor.temperature;
  if (temp > 30.0) {
    Serial.println("⚠️  WARNING: High Temperature!");
  }

  // Beispiel 5: Batterie-Warnung
  bool batteryWarning = isIndoor ? dataIndoor.battery_warning : dataOutdoor.battery_warning;
  if (batteryWarning) {
    Serial.println("⚠️  WARNING: Sensor battery is low!");
    // sendNotification("Sensor battery low!");
  }
}

// ==================== SETUP ====================

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n\n╔════════════════════════════════════════════╗");
  Serial.println("║  ESP32-C3 ESP-NOW Receiver              ║");
  Serial.println("║  Supports Indoor & Outdoor Sensors      ║");
  Serial.println("╚════════════════════════════════════════════╝\n");

  // WiFi im Station Mode starten
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  // WiFi Kanal setzen (wichtig!)
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);

  Serial.println("=== Configuration ===");
  Serial.print("WiFi Channel: ");
  Serial.println(ESPNOW_CHANNEL);
  Serial.print("MAC Address: ");
  Serial.println(WiFi.macAddress());
  Serial.println();

  // ESP-NOW initialisieren
  if (esp_now_init() != ESP_OK) {
    Serial.println("❌ ESP-NOW init failed!");
    ESP.restart();
    return;
  }
  Serial.println("✓ ESP-NOW initialized");

  // Receive Callback registrieren
  esp_now_register_recv_cb(onDataRecv);
  Serial.println("✓ Receive callback registered");

  Serial.println("\n🎧 Listening for ESP-NOW data...\n");
  Serial.println("Tip: Use this MAC address in sender config for direct messaging:");
  Serial.print("   uint8_t receiverMAC[] = {");
  uint8_t mac[6];
  WiFi.macAddress(mac);
  for (int i = 0; i < 6; i++) {
    Serial.printf("0x%02X", mac[i]);
    if (i < 5) Serial.print(", ");
  }
  Serial.println("};\n");
}

// ==================== LOOP ====================

void loop() {
  // Heartbeat - zeigt dass Empfänger läuft
  static unsigned long lastPrint = 0;
  static int dotCount = 0;

  if (millis() - lastPrint > 30000) {  // Alle 30 Sekunden
    dotCount++;
    Serial.print(".");

    if (lastReceiveTime > 0) {
      unsigned long timeSinceLastReceive = (millis() - lastReceiveTime) / 1000;
      Serial.printf(" (Last: %lus ago, Total: %d packets)",
                    timeSinceLastReceive, receivedPackets);
    } else {
      Serial.print(" (Waiting for first packet...)");
    }
    Serial.println();

    // Status alle 5 Minuten
    if (dotCount % 10 == 0) {
      Serial.println("\n--- Status ---");
      Serial.printf("Uptime: %lu seconds\n", millis() / 1000);
      Serial.printf("Packets received: %d\n", receivedPackets);
      Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
      Serial.println();
    }

    lastPrint = millis();
  }

  delay(100);
}
