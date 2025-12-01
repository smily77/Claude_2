/*
 * ESP-NOW Empfänger für ESP8266
 * Empfängt Daten von Indoor- und Outdoor-Sensoren
 *
 * Unterstützt:
 * - Outdoor-Sensor (nur BMP180)
 * - Indoor-Sensor (BMP180 + AM2321 mit Luftfeuchtigkeit)
 */

#include <ESP8266WiFi.h>
#include <espnow.h>

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
} sensor_data_outdoor;

sensor_data_indoor dataIndoor;
sensor_data_outdoor dataOutdoor;
unsigned long lastReceiveTime = 0;
int packetsReceived = 0;

// ==================== FUNKTIONEN ====================

// ESP-NOW Receive Callback
void onDataRecv(uint8_t *mac_addr, uint8_t *data, uint8_t data_len) {
  packetsReceived++;

  Serial.println("\n========================================");
  Serial.printf("Data Received! (Packet #%d)\n", packetsReceived);
  Serial.println("========================================");

  // Sender MAC anzeigen
  Serial.print("From MAC: ");
  for (int i = 0; i < 6; i++) {
    Serial.printf("%02X", mac_addr[i]);
    if (i < 5) Serial.print(":");
  }
  Serial.println();

  // Sensor-Typ erkennen (letztes Byte der Daten = sensor_type)
  // Outdoor: 20 bytes, Indoor: 25 bytes
  bool isIndoor = (data_len >= sizeof(sensor_data_indoor));

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

    Serial.print("AM2321 Readings: ");
    Serial.println(dataIndoor.am2321_readings);

    Serial.print("Battery: ");
    Serial.print(dataIndoor.battery_voltage);
    Serial.print(" mV (");
    Serial.print(dataIndoor.battery_voltage / 1000.0, 2);
    Serial.print(" V)");
    if (dataIndoor.battery_warning) {
      Serial.print(" [LOW BATTERY!]");
    }
    Serial.println();

    Serial.print("Duration: ");
    Serial.print(dataIndoor.duration);
    Serial.println(" ms");

    Serial.print("Sensor Error: ");
    Serial.println(dataIndoor.sensor_error);

    Serial.print("Reset Reason: ");
    Serial.println(dataIndoor.reset_reason);

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

    Serial.print("Battery: ");
    Serial.print(dataOutdoor.battery_voltage);
    Serial.print(" mV (");
    Serial.print(dataOutdoor.battery_voltage / 1000.0, 2);
    Serial.print(" V)");
    if (dataOutdoor.battery_warning) {
      Serial.print(" [LOW BATTERY!]");
    }
    Serial.println();

    Serial.print("Duration: ");
    Serial.print(dataOutdoor.duration);
    Serial.println(" ms");

    Serial.print("Sensor Error: ");
    Serial.println(dataOutdoor.sensor_error);

    Serial.print("Reset Reason: ");
    Serial.println(dataOutdoor.reset_reason);
  }

  Serial.println("========================================\n");

  lastReceiveTime = millis();

  // Hier können Sie die Daten weiterverarbeiten:
  // - An MQTT senden
  // - In Datenbank speichern
  // - An Display ausgeben
  // - etc.
}

// ==================== SETUP ====================

void setup() {
  Serial.begin(9600);
  delay(100);
  Serial.println("\n\n=== ESP-NOW Receiver (ESP8266) ===");
  Serial.println("Supports Indoor and Outdoor sensors");

  // WiFi im Station Mode starten
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  // WiFi Kanal setzen
  wifi_set_channel(ESPNOW_CHANNEL);

  Serial.print("WiFi Channel: ");
  Serial.println(wifi_get_channel());
  Serial.print("MAC Address: ");
  Serial.println(WiFi.macAddress());
  Serial.println("\nWaiting for data...\n");

  // ESP-NOW initialisieren
  if (esp_now_init() != 0) {
    Serial.println("ESP-NOW init failed!");
    return;
  }

  // Rolle setzen
  esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);

  // Receive Callback registrieren
  esp_now_register_recv_cb(onDataRecv);
}

// ==================== LOOP ====================

void loop() {
  // Heartbeat - zeigt dass Empfänger läuft
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 30000) {  // Alle 30 Sekunden
    Serial.print(".");
    lastPrint = millis();

    if (lastReceiveTime > 0) {
      unsigned long timeSinceLastReceive = (millis() - lastReceiveTime) / 1000;
      Serial.print(" (Last: ");
      Serial.print(timeSinceLastReceive);
      Serial.print("s ago, Total: ");
      Serial.print(packetsReceived);
      Serial.print(" packets)");
    } else {
      Serial.print(" (Waiting for first packet...)");
    }
    Serial.println();
  }

  delay(100);
}
