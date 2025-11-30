/*
 * ESP-NOW Empfänger Beispiel
 * Empfängt Daten vom ESP8266 Sensor Node und zeigt sie an
 *
 * Kann auf ESP8266 oder ESP32 laufen
 */

#include <ESP8266WiFi.h>
#include <espnow.h>

// ==================== KONFIGURATION ====================

// WiFi Kanal (MUSS mit Sender übereinstimmen!)
#define ESPNOW_CHANNEL 1

// ==================== DATENSTRUKTUR ====================
// MUSS identisch mit Sender sein!

typedef struct sensor_data {
  uint32_t timestamp;
  float temperature;
  float pressure;
  uint16_t battery_voltage;
  uint16_t duration;
  uint8_t battery_warning;
  uint8_t sensor_error;
  uint8_t reset_reason;
  uint8_t reserved;
} sensor_data;

sensor_data receivedData;
unsigned long lastReceiveTime = 0;

// ==================== FUNKTIONEN ====================

// ESP-NOW Receive Callback
void onDataRecv(uint8_t *mac_addr, uint8_t *data, uint8_t data_len) {
  Serial.println("\n========================================");
  Serial.println("New Data Received!");
  Serial.println("========================================");

  // Sender MAC anzeigen
  Serial.print("From MAC: ");
  for (int i = 0; i < 6; i++) {
    Serial.printf("%02X", mac_addr[i]);
    if (i < 5) Serial.print(":");
  }
  Serial.println();

  // Daten kopieren
  memcpy(&receivedData, data, sizeof(receivedData));

  // Daten anzeigen
  Serial.println("\n--- Sensor Data ---");
  Serial.print("Timestamp: ");
  Serial.print(receivedData.timestamp);
  Serial.println(" ms");

  Serial.print("Temperature: ");
  Serial.print(receivedData.temperature, 2);
  Serial.println(" °C");

  Serial.print("Pressure: ");
  Serial.print(receivedData.pressure, 2);
  Serial.println(" mbar");

  Serial.print("Battery: ");
  Serial.print(receivedData.battery_voltage);
  Serial.print(" mV");
  if (receivedData.battery_warning) {
    Serial.print(" [LOW BATTERY WARNING]");
  }
  Serial.println();

  Serial.print("Duration: ");
  Serial.print(receivedData.duration);
  Serial.println(" ms");

  Serial.print("Sensor Error: ");
  Serial.println(receivedData.sensor_error);

  Serial.print("Reset Reason: ");
  Serial.println(receivedData.reset_reason);

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
  Serial.begin(115200);
  Serial.println("\n\n=== ESP-NOW Receiver ===");

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
      Serial.print(" (Last received ");
      Serial.print(timeSinceLastReceive);
      Serial.print("s ago)");
    }
    Serial.println();
  }

  delay(100);
}
