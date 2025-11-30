/*
 * ESP8266 ESP-NOW Sensor Node
 * Liest BMP180 Sensor, sendet Daten via ESP-NOW und geht in Deep Sleep
 *
 * Hardware: ESP8266 ESP1 mit BMP180 Sensor
 *
 * Verbindungen:
 * - GPIO0 (SDA) -> BMP180 SDA
 * - GPIO2 (SCL) -> BMP180 SCL
 * - RST -> GPIO16 (für Deep Sleep Wake-up)
 */

#include <ESP8266WiFi.h>
#include <espnow.h>
#include <Wire.h>
#include <SFE_BMP180.h>

// ==================== KONFIGURATION ====================

// Debug Mode (true = serielle Ausgabe, false = produktiv)
#define DEBUG false

// Empfänger MAC-Adresse (ändern Sie dies auf die MAC Ihres Empfängers!)
// Für Broadcast verwenden Sie: {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}
// Für spezifischen Empfänger: {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}
uint8_t receiverMAC[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};  // Broadcast

// Sleep Zeit in Minuten
#define SLEEP_TIME_MINUTES 15

// Battery Protection
#define BATTERY_LIMIT 2600        // mV - unter diesem Wert wird länger geschlafen
#define BATTERY_EXTRA_CYCLES 2    // Multiplikator für Sleep-Zeit bei niedrigem Akku
#define BATTERY_WARNING_OFFSET 50 // mV Offset für Battery Warning Flag

// WiFi Kanal für ESP-NOW (1-13, muss mit Empfänger übereinstimmen!)
#define ESPNOW_CHANNEL 1

// I2C Pins für BMP180
#define I2C_SDA 0  // GPIO0
#define I2C_SCL 2  // GPIO2

// ==================== DATENSTRUKTUR ====================

// Struktur für Sensordaten (max 250 Bytes für ESP-NOW)
typedef struct sensor_data {
  uint32_t timestamp;        // Millisekunden seit Start (für Synchronisation)
  float temperature;         // Temperatur in °C
  float pressure;            // Luftdruck in mbar
  uint16_t battery_voltage;  // Batteriespannung in mV
  uint16_t duration;         // Messzeit in ms (aus RTC Memory)
  uint8_t battery_warning;   // 1 = Batterie niedrig, 0 = OK
  uint8_t sensor_error;      // 0 = OK, >0 = Fehlercode
  uint8_t reset_reason;      // Grund für letzten Reset
  uint8_t reserved;          // Reserviert für zukünftige Nutzung
} sensor_data;

// ==================== GLOBALE VARIABLEN ====================

extern "C" {
  #include "user_interface.h"
}
ADC_MODE(ADC_VCC);  // VCC Reading aktivieren

SFE_BMP180 bmp180;
sensor_data sensorData;
unsigned long startTime;
int batteryProtector = 1;
byte rtcStore[2];

// ==================== FUNKTIONEN ====================

// Schalte nicht verwendete Pins aus (Stromsparen)
void turnOffUnusedPins() {
  const int unusedPins[] = {4, 5, 12, 13, 14, 15};
  for (int i = 0; i < 6; i++) {
    pinMode(unusedPins[i], OUTPUT);
    digitalWrite(unusedPins[i], LOW);
  }
}

// BMP180 Sensor auslesen
int readBMP180(double &temp, double &pressure) {
  char status;

  if (!bmp180.begin()) {
    if (DEBUG) Serial.println("BMP180 init failed!");
    return 1;
  }

  // Temperatur lesen
  status = bmp180.startTemperature();
  if (status == 0) {
    if (DEBUG) Serial.println("Temperature read error");
    return 2;
  }
  delay(status);
  status = bmp180.getTemperature(temp);
  if (status == 0) {
    if (DEBUG) Serial.println("Temperature retrieve error");
    return 3;
  }

  // Druck lesen (3 = höchste Auflösung)
  status = bmp180.startPressure(3);
  if (status == 0) {
    if (DEBUG) Serial.println("Pressure read error");
    return 4;
  }
  delay(status);
  status = bmp180.getPressure(pressure, temp);
  if (status == 0) {
    if (DEBUG) Serial.println("Pressure retrieve error");
    return 5;
  }

  return 0;  // Erfolg
}

// ESP-NOW Send Callback
void onDataSent(uint8_t *mac_addr, uint8_t sendStatus) {
  if (DEBUG) {
    Serial.print("Send Status: ");
    Serial.println(sendStatus == 0 ? "Success" : "Failed");
  }
}

// ==================== SETUP ====================

void setup() {
  startTime = millis();

  // Serielle Kommunikation starten (nur wenn DEBUG)
  if (DEBUG) {
    Serial.begin(9600);
    while (!Serial && millis() < 2000);  // Warte max 2s auf Serial
    Serial.println("\n\n=== ESP8266 ESP-NOW Sensor Node ===");
  }

  // Batteriespannung prüfen
  uint16_t batteryVoltage = ESP.getVcc();
  if (DEBUG) {
    Serial.print("Battery: ");
    Serial.print(batteryVoltage);
    Serial.println(" mV");
  }

  // Bei sehr niedriger Batterie: sofort schlafen
  if (batteryVoltage < BATTERY_LIMIT) {
    if (DEBUG) Serial.println("Battery critical! Going to extended sleep...");
    batteryProtector = BATTERY_EXTRA_CYCLES;
    system_deep_sleep_set_option(WAKE_RFCAL);
    system_deep_sleep(SLEEP_TIME_MINUTES * 60UL * batteryProtector * 1000000UL);
  }

  // Reset Grund lesen
  rst_info* rinfo = ESP.getResetInfoPtr();
  byte resetReason = rinfo->reason;

  // Duration aus RTC Memory lesen
  system_rtc_mem_read(28, rtcStore, 2);
  uint16_t lastDuration = rtcStore[0] * 256 + rtcStore[1];

  if (DEBUG) {
    Serial.print("Reset reason: ");
    Serial.println(resetReason);
    Serial.print("Last duration: ");
    Serial.print(lastDuration);
    Serial.println(" ms");
  }

  // Nicht verwendete Pins ausschalten
  turnOffUnusedPins();

  // I2C initialisieren
  Wire.begin(I2C_SDA, I2C_SCL);

  // BMP180 Sensor auslesen
  double temp = 0, press = 0;
  int sensorError = readBMP180(temp, press);

  if (DEBUG) {
    Serial.println("\n--- Sensor Data ---");
    Serial.print("Temperature: ");
    Serial.print(temp);
    Serial.println(" °C");
    Serial.print("Pressure: ");
    Serial.print(press);
    Serial.println(" mbar");
    Serial.print("Sensor error code: ");
    Serial.println(sensorError);
  }

  // Datenstruktur füllen
  sensorData.timestamp = millis();
  sensorData.temperature = (float)temp;
  sensorData.pressure = (float)press;
  sensorData.battery_voltage = batteryVoltage;
  sensorData.duration = lastDuration;
  sensorData.battery_warning = (batteryVoltage < (BATTERY_LIMIT + BATTERY_WARNING_OFFSET)) ? 1 : 0;
  sensorData.sensor_error = sensorError;
  sensorData.reset_reason = resetReason;
  sensorData.reserved = 0;

  // WiFi im Station Mode starten (erforderlich für ESP-NOW)
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  // WiFi Kanal setzen
  wifi_set_channel(ESPNOW_CHANNEL);

  if (DEBUG) {
    Serial.print("WiFi Channel: ");
    Serial.println(wifi_get_channel());
    Serial.print("MAC Address: ");
    Serial.println(WiFi.macAddress());
  }

  // ESP-NOW initialisieren
  if (esp_now_init() != 0) {
    if (DEBUG) Serial.println("ESP-NOW init failed!");
    goto sleep_now;
  }

  // ESP-NOW Rolle setzen
  esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);

  // Callback registrieren
  esp_now_register_send_cb(onDataSent);

  // Empfänger hinzufügen
  int addPeerResult = esp_now_add_peer(receiverMAC, ESP_NOW_ROLE_SLAVE, ESPNOW_CHANNEL, NULL, 0);
  if (addPeerResult != 0) {
    if (DEBUG) {
      Serial.print("Failed to add peer, error: ");
      Serial.println(addPeerResult);
    }
  }

  // Daten senden
  if (DEBUG) {
    Serial.println("\n--- Sending Data via ESP-NOW ---");
    Serial.print("Receiver MAC: ");
    for (int i = 0; i < 6; i++) {
      Serial.printf("%02X", receiverMAC[i]);
      if (i < 5) Serial.print(":");
    }
    Serial.println();
    Serial.print("Data size: ");
    Serial.print(sizeof(sensorData));
    Serial.println(" bytes");
  }

  uint8_t sendResult = esp_now_send(receiverMAC, (uint8_t *)&sensorData, sizeof(sensorData));

  if (DEBUG) {
    Serial.print("Send result: ");
    Serial.println(sendResult == 0 ? "Queued" : "Failed");
  }

  // Kurz warten damit Daten gesendet werden
  delay(100);

sleep_now:
  // Duration speichern
  uint16_t duration = millis() - startTime;
  rtcStore[0] = duration / 256;
  rtcStore[1] = duration % 256;
  system_rtc_mem_write(28, rtcStore, 2);

  if (DEBUG) {
    Serial.print("\nTotal duration: ");
    Serial.print(duration);
    Serial.println(" ms");
    Serial.print("Going to sleep for ");
    Serial.print(SLEEP_TIME_MINUTES * batteryProtector);
    Serial.println(" minutes...\n");
    delay(100);  // Zeit für Serial Output
  }

  // Deep Sleep
  system_deep_sleep_set_option(WAKE_RFCAL);
  system_deep_sleep(SLEEP_TIME_MINUTES * 60UL * batteryProtector * 1000000UL);

  delay(1000);  // Sollte nie erreicht werden
}

// ==================== LOOP ====================

void loop() {
  // Wird nie erreicht, da ESP im Deep Sleep ist
}
