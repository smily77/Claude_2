/*
 * ESP8266 ESP-NOW Sensor Node (Indoor/Outdoor)
 * Unterstützt Indoor- und Outdoor-Sensoren mit einer Code-Basis
 *
 * OUTDOOR: ESP8266 ESP-01 mit BMP180 (Temperatur + Luftdruck)
 * INDOOR:  ESP8266 ESP-03 mit BMP180 + AM2321 (Temp + Druck + Luftfeuchtigkeit)
 *
 * Hardware-Verbindungen (beide):
 * - GPIO0 (SDA) -> Sensoren SDA
 * - GPIO2 (SCL) -> Sensoren SCL
 * - RST -> GPIO16 (für Deep Sleep Wake-up)
 */

// ==================== SENSOR TYP AUSWAHL ====================
// ⚠️ WICHTIG: Kommentieren Sie die gewünschte Variante ein/aus!

//#define INDOOR    // ESP-03 mit BMP180 + AM2321, sendet alle 60 Sekunden
#define OUTDOOR   // ESP-01 mit nur BMP180, sendet alle 15 Minuten

// ===========================================================

#include <ESP8266WiFi.h>
#include <espnow.h>
#include <Wire.h>
#include <SFE_BMP180.h>

#ifdef INDOOR
  #include <AM2321.h>
#endif

// ==================== KONFIGURATION ====================

// Debug Mode (true = serielle Ausgabe, false = produktiv)
#define DEBUG false

// Empfänger MAC-Adresse (ändern Sie dies auf die MAC Ihres Empfängers!)
// Für Broadcast verwenden Sie: {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}
// Für spezifischen Empfänger: {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}
uint8_t receiverMAC[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};  // Broadcast

// Sleep Zeit
#ifdef INDOOR
  #define SLEEP_TIME_SECONDS 60       // Indoor: alle 60 Sekunden
#else
  #define SLEEP_TIME_MINUTES 15       // Outdoor: alle 15 Minuten
#endif

// Battery Protection (bei niedrigem Akku länger schlafen)
#define BATTERY_LIMIT 2600        // mV - unter diesem Wert wird länger geschlafen
#ifdef INDOOR
  #define BATTERY_EXTRA_CYCLES 30   // Indoor: 30 * 60s = 30 Minuten
#else
  #define BATTERY_EXTRA_CYCLES 2    // Outdoor: 2 * 15min = 30 Minuten
#endif
#define BATTERY_WARNING_OFFSET 50 // mV Offset für Battery Warning Flag

// WiFi Kanal für ESP-NOW (1-13, muss mit Empfänger übereinstimmen!)
#define ESPNOW_CHANNEL 1

// Adaptive Sleep Konfiguration
#define MIN_SLEEP_PERIOD 20           // Minimum 20 Sekunden
#define TEMP_CHANGE_FAST 1.0          // >= 1°C: Periode verkürzen
#define TEMP_CHANGE_STABLE 0.5        // < 0.5°C: Periode verlängern
#define PERIOD_DIVIDER 3              // Faktor zum Verkürzen/Verlängern

// Standard-Perioden in Sekunden
#ifdef INDOOR
  #define DEFAULT_PERIOD 60           // Indoor: 60 Sekunden
#else
  #define DEFAULT_PERIOD 900          // Outdoor: 900 Sekunden (15 Min)
#endif

// I2C Pins für Sensoren
#define I2C_SDA 0  // GPIO0
#define I2C_SCL 2  // GPIO2

// ==================== DATENSTRUKTUR ====================

// Struktur für Sensordaten (max 250 Bytes für ESP-NOW)
typedef struct sensor_data {
  uint32_t timestamp;        // Millisekunden seit Start
  float temperature;         // Temperatur in °C (BMP180 oder AM2321)
  float pressure;            // Luftdruck in mbar (BMP180)
  #ifdef INDOOR
    float humidity;          // Luftfeuchtigkeit in % (nur Indoor: AM2321)
    uint8_t am2321_readings; // Anzahl Leseversuche AM2321 (nur Indoor)
  #endif
  uint16_t battery_voltage;  // Batteriespannung in mV
  uint16_t duration;         // Messzeit in ms (aus RTC Memory)
  uint8_t battery_warning;   // 1 = Batterie niedrig, 0 = OK
  uint8_t sensor_error;      // 0 = OK, >0 = Fehlercode
  uint8_t reset_reason;      // Grund für letzten Reset
  uint8_t sensor_type;       // 0 = Outdoor, 1 = Indoor
} sensor_data;

// RTC Memory Struktur (überlebt Deep Sleep)
typedef struct {
  uint16_t duration;           // Messzeit in ms
  float last_temperature;      // Letzte Temperatur für Vergleich
  uint16_t current_period;     // Aktuelle Sleep-Periode in Sekunden
  uint8_t is_valid;            // 0xAA = Daten gültig
} rtc_data_t;

// ==================== GLOBALE VARIABLEN ====================

extern "C" {
  #include "user_interface.h"
}
ADC_MODE(ADC_VCC);  // VCC Reading aktivieren

SFE_BMP180 bmp180;
#ifdef INDOOR
  AM2321 am2321;
#endif

sensor_data sensorData;
rtc_data_t rtcData;
unsigned long startTime;
int batteryProtector = 1;
uint16_t sleepPeriod = DEFAULT_PERIOD;  // Aktuelle Sleep-Periode in Sekunden

// ==================== FUNKTIONEN ====================

// Schalte nicht verwendete Pins aus (Stromsparen)
void turnOffUnusedPins() {
  const int unusedPins[] = {4, 5, 12, 13, 14, 15};
  for (int i = 0; i < 6; i++) {
    pinMode(unusedPins[i], OUTPUT);
    digitalWrite(unusedPins[i], LOW);
  }
}

#ifdef INDOOR
// AM2321 Sensor auslesen (nur Indoor)
int readAM2321(float &temp, float &hum) {
  int readings = 1;

  while (!am2321.read()) {
    readings++;
    if (readings > 10) {
      // Nach 10 Versuchen abbrechen
      temp = -999.0;
      hum = -999.0;
      return readings;
    }
    delay(100);
  }

  temp = am2321.temperature / 10.0;
  hum = am2321.humidity / 10.0;

  return readings;
}
#endif

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

// RTC Memory laden
void loadRTCData() {
  // RTC Daten aus Block 64 lesen (User-Bereich)
  system_rtc_mem_read(64, (uint32_t*)&rtcData, sizeof(rtcData));

  // Validierung
  if (rtcData.is_valid != 0xAA) {
    // Erste Initialisierung oder ungültige Daten
    rtcData.duration = 0;
    rtcData.last_temperature = 20.0;  // Annahme: 20°C als Start
    rtcData.current_period = DEFAULT_PERIOD;
    rtcData.is_valid = 0xAA;

    if (DEBUG) Serial.println("RTC Data initialized");
  } else {
    if (DEBUG) {
      Serial.print("RTC Data loaded - Last temp: ");
      Serial.print(rtcData.last_temperature);
      Serial.print("°C, Period: ");
      Serial.print(rtcData.current_period);
      Serial.println("s");
    }
  }
}

// RTC Memory speichern
void saveRTCData() {
  rtcData.is_valid = 0xAA;
  system_rtc_mem_write(64, (uint32_t*)&rtcData, sizeof(rtcData));
}

// Adaptive Sleep-Periode berechnen
uint16_t calculateAdaptivePeriod(float current_temp, float last_temp, uint16_t current_period) {
  // DEBUG Mode: immer 20 Sekunden
  if (DEBUG) {
    if (DEBUG) Serial.println("DEBUG Mode: Fixed 20s period");
    return 20;
  }

  // Battery Protection Mode: keine Dynamisierung
  if (batteryProtector > 1) {
    if (DEBUG) Serial.println("Battery Protection: No adaptation");
    return DEFAULT_PERIOD;
  }

  // Temperaturänderung berechnen
  float temp_change = abs(current_temp - last_temp);
  uint16_t new_period = current_period;

  if (temp_change >= TEMP_CHANGE_FAST) {
    // Temperatur ändert sich schnell: Periode verkürzen
    new_period = current_period / PERIOD_DIVIDER;
    if (new_period < MIN_SLEEP_PERIOD) {
      new_period = MIN_SLEEP_PERIOD;
    }

    if (DEBUG) {
      Serial.print("Temp change ");
      Serial.print(temp_change, 2);
      Serial.print("°C >= ");
      Serial.print(TEMP_CHANGE_FAST);
      Serial.print("°C → Period shortened to ");
      Serial.print(new_period);
      Serial.println("s");
    }

  } else if (temp_change < TEMP_CHANGE_STABLE) {
    // Temperatur stabil: Periode verlängern
    new_period = current_period * PERIOD_DIVIDER;
    if (new_period > DEFAULT_PERIOD) {
      new_period = DEFAULT_PERIOD;
    }

    if (DEBUG) {
      Serial.print("Temp change ");
      Serial.print(temp_change, 2);
      Serial.print("°C < ");
      Serial.print(TEMP_CHANGE_STABLE);
      Serial.print("°C → Period extended to ");
      Serial.print(new_period);
      Serial.println("s");
    }

  } else {
    if (DEBUG) {
      Serial.print("Temp change ");
      Serial.print(temp_change, 2);
      Serial.print("°C → Period unchanged: ");
      Serial.print(new_period);
      Serial.println("s");
    }
  }

  return new_period;
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

  // Variablen deklarieren (vor goto Label)
  int addPeerResult = 0;
  uint8_t sendResult = 0;

  // Serielle Kommunikation starten (nur wenn DEBUG)
  if (DEBUG) {
    Serial.begin(9600);
    while (!Serial && millis() < 2000);  // Warte max 2s auf Serial
    #ifdef INDOOR
      Serial.println("\n\n=== ESP8266 ESP-NOW INDOOR Sensor ===");
    #else
      Serial.println("\n\n=== ESP8266 ESP-NOW OUTDOOR Sensor ===");
    #endif
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
    #ifdef INDOOR
      system_deep_sleep(SLEEP_TIME_SECONDS * batteryProtector * 1000000UL);
    #else
      system_deep_sleep(SLEEP_TIME_MINUTES * 60UL * batteryProtector * 1000000UL);
    #endif
  }

  // Reset Grund lesen
  rst_info* rinfo = ESP.getResetInfoPtr();
  byte resetReason = rinfo->reason;

  // RTC Memory laden (enthält duration, last_temp, current_period)
  loadRTCData();

  if (DEBUG) {
    Serial.print("Reset reason: ");
    Serial.println(resetReason);
    Serial.print("Last duration: ");
    Serial.print(rtcData.duration);
    Serial.println(" ms");
  }

  // Nicht verwendete Pins ausschalten
  turnOffUnusedPins();

  // I2C initialisieren
  Wire.begin(I2C_SDA, I2C_SCL);

  // Sensoren auslesen
  double temp = 0, press = 0;
  int sensorError = readBMP180(temp, press);

  #ifdef INDOOR
    // Indoor: auch AM2321 auslesen
    float tempAM2321 = 0, humidity = 0;
    int am2321Readings = readAM2321(tempAM2321, humidity);

    if (DEBUG) {
      Serial.println("\n--- Sensor Data (Indoor) ---");
      Serial.print("BMP180 Temperature: ");
      Serial.print(temp);
      Serial.println(" °C");
      Serial.print("BMP180 Pressure: ");
      Serial.print(press);
      Serial.println(" mbar");
      Serial.print("AM2321 Temperature: ");
      Serial.print(tempAM2321);
      Serial.println(" °C");
      Serial.print("AM2321 Humidity: ");
      Serial.print(humidity);
      Serial.println(" %");
      Serial.print("AM2321 Readings: ");
      Serial.println(am2321Readings);
      Serial.print("BMP180 error code: ");
      Serial.println(sensorError);
    }

    // Verwende AM2321 Temperatur wenn verfügbar, sonst BMP180
    if (tempAM2321 > -990.0) {
      temp = tempAM2321;
    }
  #else
    // Outdoor: nur BMP180
    if (DEBUG) {
      Serial.println("\n--- Sensor Data (Outdoor) ---");
      Serial.print("Temperature: ");
      Serial.print(temp);
      Serial.println(" °C");
      Serial.print("Pressure: ");
      Serial.print(press);
      Serial.println(" mbar");
      Serial.print("Sensor error code: ");
      Serial.println(sensorError);
    }
  #endif

  // Adaptive Sleep-Periode berechnen
  sleepPeriod = calculateAdaptivePeriod((float)temp, rtcData.last_temperature, rtcData.current_period);

  if (DEBUG) {
    Serial.println("\n--- Adaptive Sleep ---");
    Serial.print("Current temp: ");
    Serial.print(temp, 2);
    Serial.print("°C, Last temp: ");
    Serial.print(rtcData.last_temperature, 2);
    Serial.println("°C");
    Serial.print("New sleep period: ");
    Serial.print(sleepPeriod);
    Serial.println(" seconds");
  }

  // Aktuelle Temperatur für nächsten Vergleich speichern
  rtcData.last_temperature = (float)temp;
  rtcData.current_period = sleepPeriod;

  // Datenstruktur füllen
  sensorData.timestamp = millis();
  sensorData.temperature = (float)temp;
  sensorData.pressure = (float)press;
  #ifdef INDOOR
    sensorData.humidity = humidity;
    sensorData.am2321_readings = am2321Readings;
    sensorData.sensor_type = 1;  // Indoor
  #else
    sensorData.sensor_type = 0;  // Outdoor
  #endif
  sensorData.battery_voltage = batteryVoltage;
  sensorData.duration = rtcData.duration;
  sensorData.battery_warning = (batteryVoltage < (BATTERY_LIMIT + BATTERY_WARNING_OFFSET)) ? 1 : 0;
  sensorData.sensor_error = sensorError;
  sensorData.reset_reason = resetReason;

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
  addPeerResult = esp_now_add_peer(receiverMAC, ESP_NOW_ROLE_SLAVE, ESPNOW_CHANNEL, NULL, 0);
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

  sendResult = esp_now_send(receiverMAC, (uint8_t *)&sensorData, sizeof(sensorData));

  if (DEBUG) {
    Serial.print("Send result: ");
    Serial.println(sendResult == 0 ? "Queued" : "Failed");
  }

  // Kurz warten damit Daten gesendet werden
  delay(100);

sleep_now:
  // Duration speichern in RTC Memory
  rtcData.duration = millis() - startTime;
  saveRTCData();

  // Tatsächliche Sleep-Zeit berechnen
  uint32_t actualSleepTime;
  if (batteryProtector > 1) {
    // Battery Protection Mode: Extended Sleep (30 Min)
    #ifdef INDOOR
      actualSleepTime = SLEEP_TIME_SECONDS * batteryProtector;
    #else
      actualSleepTime = SLEEP_TIME_MINUTES * 60UL * batteryProtector;
    #endif
  } else if (DEBUG) {
    // Debug Mode: 20 Sekunden
    actualSleepTime = 20;
  } else {
    // Normal Mode: Adaptive Periode
    actualSleepTime = sleepPeriod;
  }

  if (DEBUG) {
    Serial.print("\nTotal duration: ");
    Serial.print(rtcData.duration);
    Serial.println(" ms");
    Serial.print("Going to sleep for ");
    Serial.print(actualSleepTime);
    Serial.print(" seconds");
    if (batteryProtector > 1) {
      Serial.print(" (Battery Protection)");
    } else if (DEBUG) {
      Serial.print(" (Debug Mode)");
    } else {
      Serial.print(" (Adaptive)");
    }
    Serial.println("...\n");
    delay(100);  // Zeit für Serial Output
  }

  // Deep Sleep mit adaptiver Periode
  system_deep_sleep_set_option(WAKE_RFCAL);
  system_deep_sleep(actualSleepTime * 1000000UL);

  delay(1000);  // Sollte nie erreicht werden
}

// ==================== LOOP ====================

void loop() {
  // Wird nie erreicht, da ESP im Deep Sleep ist
}
