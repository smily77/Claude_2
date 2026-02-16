// Desk_Top_Widget_III
// ESP32-C3 Super Mini + A7670E LTE Version
//
// Migration von Desk_Top_Widget_II_opt_G4:
// - ESP32-C3 Super Mini statt ESP8266
// - A7670E LTE-Modul (eingebaut) statt externem 4G-Router
// - Kein WiFi mehr - direkte LTE-Verbindung ueber AT-Kommandos
// - Kein WiFi Keep-Alive noetig (A7670E haelt LTE-Verbindung selbst)
// - PWM via LEDC statt analogWrite
// - 12-bit ADC statt 10-bit

#include <TimeLib.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SFE_BMP180.h>
#include <Wire.h>
#include <Ticker.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include "AirportDatabase.h"

#define DEBUG false

// ============================================
// KONFIGURATION: Nur Airport-Codes aendern!
// ============================================
const char* AIRPORT_CODES[7] = {
  "ZRH",  // 0: Lokale Zeit (Zuerich, Schweiz)
  "DXB",  // 1: Dubai, VAE
  "SIN",  // 2: Singapore
  "IAD",  // 3: Washington DC, USA
  "SYD",  // 4: Sydney, Australien
  "BLR",  // 5: Bangalore, Indien
  "SFO"   // 6: San Francisco, USA
};
// ============================================

// ============================================
// A7670E KONFIGURATION
// ============================================
// APN anpassen an den Mobilfunkanbieter der SIM-Karte:
// Beispiele: "internet" (generisch), "gprs.swisscom.ch" (Swisscom),
//            "internet.telekom" (Telekom DE), "web.vodafone.de" (Vodafone DE)
const char* APN = "internet";
const char* APN_USER = "";   // Meist leer
const char* APN_PASS = "";   // Meist leer
// ============================================

// ============================================
// HELLIGKEITSSTEUERUNG (ESP32-C3: 12-bit ADC, 0-4095)
// ============================================
#define LDR_HELL   1600  // ADC-Wert bei hellem Licht
#define LDR_DUNKEL 4095  // ADC-Wert bei Dunkelheit
#define PWM_HELL   0     // PWM fuer hellstes Display
#define PWM_DUNKEL 253   // PWM fuer dunklestes Display (gerade noch sichtbar)
// ============================================

// ============================================
// PIN-DEFINITIONEN ESP32-C3 Super Mini
// ============================================
// SPI -> TFT Display
#define TFT_PIN_CS   7
#define TFT_PIN_DC   0
#define TFT_PIN_RST  1
#define SPI_MOSI     6
#define SPI_SCK      4

// LED Hintergrundbeleuchtung (PWM)
#define LED_PIN      5

// I2C -> BMP180
#define I2C_SDA      8
#define I2C_SCL      9

// UART -> A7670E
#define A7670E_TX    21  // ESP TX -> A7670E RX
#define A7670E_RX    20  // ESP RX <- A7670E TX
#define A7670E_PWRKEY 10
#define A7670E_RST_PIN 2

// ADC -> LDR
#define LDR_PIN      3
// ============================================

// A7670E Serial (UART1)
HardwareSerial A7670E_Serial(1);

SFE_BMP180 pressure;
char status;
double T, P;

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_PIN_CS, TFT_PIN_DC, TFT_PIN_RST);

time_t currentTime = 0;
char anzeige[24];
int secondLast, minuteLast;
boolean firstRun = true;

// Timezone-Strukturen
struct TimezoneInfo {
  String airCode;
  int stdOffset;
  int dstOffset;
  byte dstType;
};

TimezoneInfo timezones[7];

// Wechselkurs-Daten
String fxSym[4] = {"CHF", "USD", "EUR", "GBP"};
float fxValue[4];
boolean currenciesValid = false;

// LTE-Status
boolean lteConnected = false;
boolean dailyUpdateDone = false;

// Watchdog
Ticker secondTick;
volatile int watchDogCount = 0;

void ISRwatchDog() {
  watchDogCount++;
  if (watchDogCount >= 60) {
    watchDogAction();
  }
}

void watchDogFeed() {
  watchDogCount = 0;
  delay(1);
}

void watchDogAction() {
  secondTick.detach();
  clearTFTScreen();
  tft.setTextColor(ST7735_RED);
  tft.setTextSize(2);
  tft.println("WATCHDOG");
  tft.println("ATTACK");
  delay(10000);
  ESP.restart();
}

// ============================================
// AT-KOMMANDO HILFSFUNKTIONEN
// ============================================

// Sendet AT-Befehl und wartet auf erwartete Antwort
// Gibt true zurueck wenn erwartete Antwort empfangen
boolean sendATCommand(const char* cmd, const char* expectedResp,
                      unsigned long timeout) {
  return sendATCommand(cmd, expectedResp, timeout, NULL);
}

boolean sendATCommand(const char* cmd, const char* expectedResp,
                      unsigned long timeout, String* responseOut) {
  // Eingangspuffer leeren
  while (A7670E_Serial.available()) {
    A7670E_Serial.read();
  }

  if (DEBUG) {
    Serial.print("AT> ");
    Serial.println(cmd);
  }

  A7670E_Serial.println(cmd);

  String response = "";
  unsigned long startTime = millis();

  while (millis() - startTime < timeout) {
    while (A7670E_Serial.available()) {
      char c = A7670E_Serial.read();
      response += c;
    }

    if (response.indexOf(expectedResp) >= 0) {
      if (DEBUG) {
        Serial.print("AT< ");
        Serial.println(response);
      }
      if (responseOut) *responseOut = response;
      return true;
    }

    if (response.indexOf("ERROR") >= 0) {
      if (DEBUG) {
        Serial.print("AT ERROR: ");
        Serial.println(response);
      }
      if (responseOut) *responseOut = response;
      return false;
    }

    watchDogFeed();
    delay(10);
  }

  if (DEBUG) {
    Serial.print("AT TIMEOUT: ");
    Serial.println(response);
  }
  if (responseOut) *responseOut = response;
  return false;
}

// A7670E einschalten (PWRKEY Puls)
void powerOnA7670E() {
  pinMode(A7670E_PWRKEY, OUTPUT);
  pinMode(A7670E_RST_PIN, OUTPUT);

  // Reset sicherstellen
  digitalWrite(A7670E_RST_PIN, HIGH);
  delay(100);

  // PWRKEY Puls: LOW fuer 1.5 Sekunden
  digitalWrite(A7670E_PWRKEY, HIGH);
  delay(100);
  digitalWrite(A7670E_PWRKEY, LOW);
  delay(1500);
  digitalWrite(A7670E_PWRKEY, HIGH);

  // Warten auf Modul-Boot
  delay(5000);
}

// A7670E initialisieren: Kommunikation pruefen, SIM, Netz registrieren
boolean initA7670E() {
  // Schritt 1: AT Kommunikation pruefen
  tft.print("AT check: ");
  boolean atOK = false;
  for (int i = 0; i < 10; i++) {
    if (sendATCommand("AT", "OK", 2000)) {
      atOK = true;
      break;
    }
    delay(1000);
  }
  if (!atOK) {
    tft.println("FAIL");
    return false;
  }
  tft.println("OK");

  // Echo ausschalten
  sendATCommand("ATE0", "OK", 2000);

  // Schritt 2: SIM-Karte pruefen
  tft.print("SIM: ");
  if (!sendATCommand("AT+CPIN?", "READY", 5000)) {
    tft.println("FAIL");
    return false;
  }
  tft.println("OK");

  // Schritt 3: Signalqualitaet pruefen
  tft.print("Signal: ");
  String csqResp;
  if (sendATCommand("AT+CSQ", "+CSQ:", 5000, &csqResp)) {
    int idx = csqResp.indexOf("+CSQ: ");
    if (idx >= 0) {
      int rssi = csqResp.substring(idx + 6, csqResp.indexOf(",", idx)).toInt();
      tft.print(rssi);
      tft.println(rssi > 5 ? " OK" : " WEAK");
      if (rssi == 99 || rssi == 0) {
        tft.println("No signal!");
        return false;
      }
    }
  } else {
    tft.println("FAIL");
    return false;
  }

  // Schritt 4: Netzregistrierung warten
  tft.print("Network: ");
  boolean registered = false;
  for (int i = 0; i < 30; i++) {
    String cregResp;
    if (sendATCommand("AT+CREG?", "+CREG:", 3000, &cregResp)) {
      if (cregResp.indexOf(",1") >= 0 || cregResp.indexOf(",5") >= 0) {
        registered = true;
        break;
      }
    }
    if (i > 0 && i % 5 == 0) tft.print(".");
    delay(2000);
    watchDogFeed();
  }
  if (!registered) {
    tft.println("FAIL");
    return false;
  }
  tft.println("OK");

  // Schritt 5: GPRS/LTE Attach pruefen
  tft.print("LTE: ");
  boolean attached = false;
  for (int i = 0; i < 10; i++) {
    if (sendATCommand("AT+CGATT?", "+CGATT: 1", 5000)) {
      attached = true;
      break;
    }
    delay(2000);
    watchDogFeed();
  }
  if (!attached) {
    tft.println("FAIL");
    return false;
  }
  tft.println("OK");

  // Schritt 6: PDP-Kontext konfigurieren (APN)
  tft.print("APN: ");
  char apnCmd[128];
  snprintf(apnCmd, sizeof(apnCmd), "AT+CGDCONT=1,\"IP\",\"%s\"", APN);
  if (!sendATCommand(apnCmd, "OK", 5000)) {
    tft.println("FAIL");
    return false;
  }
  tft.println(APN);

  // PDP-Kontext aktivieren
  sendATCommand("AT+CGACT=1,1", "OK", 10000);

  lteConnected = true;
  return true;
}

// ============================================
// SETUP
// ============================================
void setup() {
  Serial.begin(115200);
  if (DEBUG) Serial.println("Desk_Top_Widget_III starting...");

  // LEDC PWM fuer Display-Hintergrundbeleuchtung
  ledcAttach(LED_PIN, 5000, 8);  // Pin, 5kHz, 8-bit Aufloesung
  ledcWrite(LED_PIN, PWM_HELL);  // Display hell beim Start

  // SPI initialisieren mit ESP32-C3 Pins
  SPI.begin(SPI_SCK, -1, SPI_MOSI, TFT_PIN_CS);

  // TFT Display initialisieren
  tft.initR(INITR_BLACKTAB);
  tft.setTextWrap(false);
  tft.setTextColor(ST7735_WHITE);
  tft.setRotation(1);
  tft.fillScreen(ST7735_BLACK);
  tft.setTextColor(ST7735_WHITE, ST7735_BLACK);
  tft.setTextSize(1);
  tft.setCursor(0, 0);
  tft.println("Desk_Top_Widget_III");
  tft.println("ESP32-C3 + A7670E");
  tft.println();

  // A7670E UART initialisieren
  A7670E_Serial.begin(115200, SERIAL_8N1, A7670E_RX, A7670E_TX);

  // A7670E einschalten
  tft.println("A7670E Power On...");
  powerOnA7670E();

  // A7670E initialisieren (SIM, Netz, APN)
  clearTFTScreen();
  tft.println("LTE Init...");
  tft.println();

  if (!initA7670E()) {
    tft.println();
    tft.println("LTE init failed!");
    tft.println("Restarting in 10s...");
    delay(10000);
    ESP.restart();
  }

  // Initialisiere Timezones
  clearTFTScreen();
  tft.println("Loading timezones...");
  initializeTimezones();

  // --- NTP Zeitsynchronisation via A7670E ---
  clearTFTScreen();
  tft.print("NTP sync: ");

  int ntpAttempts = 0;
  while (currentTime == 0 && ntpAttempts < 4) {
    ntpAttempts++;
    currentTime = getNtpTimeA7670E();
    if (currentTime == 0) {
      tft.print(".");
      delay(2000);
    }
  }

  if (currentTime != 0) {
    tft.println("OK!");
  } else {
    tft.println("FAIL");

    // HTTP Fallback
    tft.print("HTTP sync: ");
    int httpAttempts = 0;
    while (currentTime == 0 && httpAttempts < 4) {
      httpAttempts++;
      currentTime = getTimeHTTP_A7670E();
      if (currentTime == 0) {
        tft.print(".");
        delay(3000);
      }
    }

    if (currentTime != 0) {
      tft.println("OK!");
    } else {
      tft.println("FAIL");
      tft.println("Time sync failed!");
      tft.println("Restarting in 10s...");
      delay(10000);
      ESP.restart();
    }
  }

  setTime(currentTime);
  tft.print("Time: ");
  tft.print(hour());
  tft.print(":");
  sprintf(anzeige, "%02i", minute());
  tft.println(anzeige);
  delay(2000);

  // Wechselkurse holen
  clearTFTScreen();
  tft.print("Currencies: ");
  currenciesValid = catchCurrencies();
  tft.println(currenciesValid ? "OK" : "FAIL");
  delay(1000);

  firstRun = false;
  secondLast = second();
  minuteLast = minute();
  displayMainScreen();

  // I2C / BMP180 initialisieren
  Wire.begin(I2C_SDA, I2C_SCL);
  pressure.begin();

  // Watchdog starten
  secondTick.attach(1.0, ISRwatchDog);
}

// ============================================
// LOOP
// ============================================
void loop() {
  // --- Display-Update jede Minute ---
  if (minuteLast != minute()) {
    minuteLast = minute();
    if (DEBUG) {
      Serial.print(hour());
      Serial.print(":");
      Serial.println(minute());
    }

    // Tages-Update um 17:00
    if (hour() == 17 && minute() == 0 && !dailyUpdateDone) {
      doDailyUpdate();
      dailyUpdateDone = true;
    }

    // Reset-Flag nach 17:00
    if (hour() == 17 && minute() > 0) {
      dailyUpdateDone = false;
    }
    if (hour() != 17) {
      dailyUpdateDone = false;
    }

    displayMainScreen();
  }

  // --- Helligkeitssteuerung (12-bit ADC + LEDC PWM) ---
  int ldrVal = analogRead(LDR_PIN);
  int pwm;
  if (ldrVal <= LDR_HELL) {
    pwm = PWM_HELL;
  } else if (ldrVal >= LDR_DUNKEL) {
    pwm = PWM_DUNKEL;
  } else {
    pwm = map(ldrVal, LDR_HELL, LDR_DUNKEL, PWM_HELL, PWM_DUNKEL);
  }
  ledcWrite(LED_PIN, pwm);
  delay(100);

  watchDogFeed();
}

// ============================================
// TAEGLICHES UPDATE (17:00)
// ============================================
void doDailyUpdate() {
  if (DEBUG) Serial.println("Daily update at 17:00...");

  // LTE-Verbindung pruefen
  String cregResp;
  if (sendATCommand("AT+CREG?", "+CREG:", 3000, &cregResp)) {
    if (cregResp.indexOf(",1") >= 0 || cregResp.indexOf(",5") >= 0) {
      lteConnected = true;
    } else {
      lteConnected = false;
      if (DEBUG) Serial.println("Daily: LTE not registered");
      return;
    }
  }

  // Zeit synchronisieren: NTP zuerst, dann HTTP Fallback
  time_t newTime = 0;

  for (int i = 0; i < 3 && newTime == 0; i++) {
    newTime = getNtpTimeA7670E();
    if (newTime == 0) delay(2000);
  }

  // HTTP Fallback
  if (newTime == 0) {
    if (DEBUG) Serial.println("Daily: NTP failed, trying HTTP...");
    for (int i = 0; i < 3 && newTime == 0; i++) {
      newTime = getTimeHTTP_A7670E();
      if (newTime == 0) delay(3000);
    }
  }

  if (newTime != 0) {
    setTime(newTime);
    if (DEBUG) Serial.println("Daily: Time synced");
  }

  // Wechselkurse aktualisieren
  currenciesValid = catchCurrencies();
  if (DEBUG) {
    Serial.print("Daily: Currencies ");
    Serial.println(currenciesValid ? "OK" : "FAILED");
  }
}
