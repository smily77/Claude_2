// Desk_Top_Widget_II_opt_G4
// G4-Version: Angepasst fuer 4G-Hotspot Betrieb
//
// Aenderungen gegenueber Desk_Top_Widget_II_opt:
// - Vereinfachte WLAN-Auswahl (ssid1 bei DEBUG, sonst ssid2, kein Umschalten)
// - WiFi Keep-Alive alle 3 Minuten (nicht-blockierend im Hintergrund)
// - Korrekte Helligkeitssteuerung (invertierte LDR-Logik)
// - Taegliche Aktualisierung um 17:00 statt Mitternacht
// - Staedtezeiten ROT bei WiFi-Fehler, Wechselkurse ausgeblendet bei Fehler

#include <TimeLib.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiUdp.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SFE_BMP180.h>
#include <Wire.h>
#include <Streaming.h>
#include <Ticker.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include "AirportDatabase.h"
extern "C" {
  #include "user_interface.h"
}

#define NEW
//#define SCHWARZ
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
// WIFI KEEP-ALIVE KONFIGURATION
// ============================================
const unsigned long KEEPALIVE_INTERVAL_MS = 3UL * 60UL * 1000UL;  // 3 Minuten
const unsigned long WIFI_CONNECT_TIMEOUT_MS = 30UL * 1000UL;       // 30 Sekunden max
// ============================================

// ============================================
// HELLIGKEITSSTEUERUNG
// ============================================
#define LDR_HELL   400   // A0-Wert bei hellem Licht
#define LDR_DUNKEL 1023  // A0-Wert bei Dunkelheit
#define PWM_HELL   0     // PWM fuer hellstes Display
#define PWM_DUNKEL 253   // PWM fuer dunklestes Display (gerade noch sichtbar)
// ============================================

SFE_BMP180 pressure;
char status;
double T, P;

WiFiClientSecure clientSec;
#include <Credentials.h>

// Vereinfachte WLAN-Auswahl: ssid1 bei DEBUG, sonst ssid2
const char* activeSSID = DEBUG ? ssid1 : ssid2;
const char* activePassword = DEBUG ? password1 : password2;

const char* timerServerDNSName = "0.ch.pool.ntp.org";
IPAddress timeServer;

WiFiUDP Udp;
const unsigned int localPort = 8888;
const int NTP_PACKET_SIZE = 48;
byte packetBuffer[NTP_PACKET_SIZE];

#define TFT_PIN_CS   15
#define TFT_PIN_DC   2
#define TFT_PIN_RST  12

#if defined(SCHWARZ)
  #define ledPin 0
#else
  #define ledPin 4
#endif

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
boolean currenciesValid = false;  // Wechselkurse erfolgreich geladen?

// WiFi Keep-Alive State Machine
enum WifiKAState {
  KA_IDLE,
  KA_CONNECTING,
  KA_CONNECTED
};

WifiKAState kaState = KA_IDLE;
unsigned long kaTimer = 0;            // Timer fuer naechsten Keep-Alive
unsigned long kaConnectStart = 0;     // Zeitpunkt des Connect-Starts
boolean lastWifiOK = true;            // Letzter Keep-Alive erfolgreich?
boolean dailyUpdateDone = false;      // Tages-Update schon gemacht?

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
  ESP.reset();
}

void setup() {
  if (DEBUG) Serial.begin(9600);

#if defined(NEW)
  pinMode(ledPin, OUTPUT);
  analogWrite(ledPin, PWM_HELL);  // Display hell beim Start
#endif

  tft.initR(INITR_BLACKTAB);
  tft.setTextWrap(false);
  tft.setTextColor(ST7735_WHITE);
#if defined(NEW)
  tft.setRotation(1);
#elif defined(OLD)
  tft.setRotation(3);
#endif
  tft.fillScreen(ST7735_BLACK);
  tft.setTextColor(ST7735_WHITE, ST7735_BLACK);
  tft.setTextSize(1);
  tft.setCursor(0, 0);

  // WiFi im Station-Modus
  WiFi.mode(WIFI_STA);

  tft.print("Connecting to: ");
  tft.println(activeSSID);
  tft.setCursor(0, 16);

  // WiFi-Verbindung (blockierend beim Start)
  WiFi.begin(activeSSID, activePassword);

  int wPeriode = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    tft.print(".");
    wPeriode++;
    if (wPeriode > 100) {
      tft.println();
      tft.println("WiFi connection failed!");
      tft.println("Restarting in 10s...");
      delay(10000);
      ESP.reset();
    }
  }

  clearTFTScreen();
  tft.println("WiFi connected");
  tft.print("IP: ");
  tft.println(WiFi.localIP());
  tft.print("MAC: ");
  tft.println(WiFi.macAddress());
  if (DEBUG) {
    Serial.print("MAC: ");
    Serial.println(WiFi.macAddress());
    Serial.println(WiFi.localIP());
  }

  // Initialisiere Timezones
  clearTFTScreen();
  tft.println("Loading timezones...");
  initializeTimezones();

  // NTP Zeit holen
  clearTFTScreen();
  while (currentTime == 0) {
    tft.print("Resolving NTP Server IP ");
    WiFi.hostByName(timerServerDNSName, timeServer);
    tft.println(timeServer.toString());

    tft.print("Starting UDP... ");
    Udp.begin(localPort);
    tft.print("local port: ");
    tft.println(Udp.localPort());

    tft.println("Waiting for NTP sync");
    currentTime = getNtpTime();
    tft.println(currentTime);
    setTime(currentTime);
  }

  // Wechselkurse holen
  clearTFTScreen();
  tft.println("Fetching currencies...");
  currenciesValid = catchCurrencies();

  // WiFi trennen - ab jetzt nur noch Keep-Alive
  WiFi.disconnect();
  if (DEBUG) Serial.println("WiFi disconnected after setup");

  firstRun = false;
  secondLast = second();
  minuteLast = minute();
  displayMainScreen();

#if defined(SCHWARZ)
  Wire.begin(4, 5);
  pressure.begin();
#else
  if (!DEBUG) {
    Wire.begin(1, 3);
    pressure.begin();
  }
#endif

  // Keep-Alive Timer starten
  kaTimer = millis();
  kaState = KA_IDLE;

  secondTick.attach(1, ISRwatchDog);
}

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

  // --- WiFi Keep-Alive (nicht-blockierend) ---
  handleKeepAlive();

  // --- Helligkeitssteuerung (korrekt) ---
#if defined(NEW)
  int ldrVal = analogRead(A0);
  int pwm;
  if (ldrVal <= LDR_HELL) {
    pwm = PWM_HELL;
  } else if (ldrVal >= LDR_DUNKEL) {
    pwm = PWM_DUNKEL;
  } else {
    pwm = map(ldrVal, LDR_HELL, LDR_DUNKEL, PWM_HELL, PWM_DUNKEL);
  }
  analogWrite(ledPin, pwm);
  delay(100);
#endif

  watchDogFeed();
}

// Nicht-blockierender WiFi Keep-Alive
void handleKeepAlive() {
  switch (kaState) {
    case KA_IDLE:
      if (millis() - kaTimer >= KEEPALIVE_INTERVAL_MS) {
        // Keep-Alive starten
        if (DEBUG) Serial.println("Keep-Alive: connecting...");
        WiFi.begin(activeSSID, activePassword);
        kaConnectStart = millis();
        kaState = KA_CONNECTING;
      }
      break;

    case KA_CONNECTING:
      if (WiFi.status() == WL_CONNECTED) {
        // Verbindung erfolgreich
        if (DEBUG) Serial.println("Keep-Alive: connected OK");
        if (!lastWifiOK) {
          lastWifiOK = true;
          displayMainScreen();  // Farbe zuruecksetzen
        }
        lastWifiOK = true;
        kaState = KA_CONNECTED;
        kaConnectStart = millis();  // Reuse als "connected since"
      } else if (millis() - kaConnectStart > WIFI_CONNECT_TIMEOUT_MS) {
        // Timeout
        if (DEBUG) Serial.println("Keep-Alive: FAILED (timeout)");
        if (lastWifiOK) {
          lastWifiOK = false;
          displayMainScreen();  // Farbe auf rot
        }
        lastWifiOK = false;
        WiFi.disconnect();
        kaTimer = millis();
        kaState = KA_IDLE;
      }
      break;

    case KA_CONNECTED:
      // 2 Sekunden verbunden bleiben damit Router es registriert
      if (millis() - kaConnectStart > 2000) {
        WiFi.disconnect();
        if (DEBUG) Serial.println("Keep-Alive: disconnected");
        kaTimer = millis();
        kaState = KA_IDLE;
      }
      break;
  }
}

// Taegliches Update (blockierend, einmal um 17:00)
void doDailyUpdate() {
  if (DEBUG) Serial.println("Daily update at 17:00...");

  // WiFi verbinden (blockierend)
  // Falls gerade ein Keep-Alive laeuft, abbrechen
  kaState = KA_IDLE;
  WiFi.disconnect();
  delay(100);

  WiFi.begin(activeSSID, activePassword);
  int waitCount = 0;
  while (WiFi.status() != WL_CONNECTED && waitCount < 60) {
    delay(500);
    waitCount++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    lastWifiOK = true;
    if (DEBUG) Serial.println("Daily: WiFi connected");

    // NTP Zeit synchronisieren
    time_t newTime = getNtpTime();
    if (newTime != 0) {
      setTime(newTime);
      if (DEBUG) Serial.println("Daily: NTP synced");
    }

    // Wechselkurse aktualisieren
    currenciesValid = catchCurrencies();
    if (DEBUG) {
      Serial.print("Daily: Currencies ");
      Serial.println(currenciesValid ? "OK" : "FAILED");
    }
  } else {
    lastWifiOK = false;
    if (DEBUG) Serial.println("Daily: WiFi FAILED");
  }

  // WiFi trennen
  WiFi.disconnect();
  if (DEBUG) Serial.println("Daily: WiFi disconnected");

  // Keep-Alive Timer neu starten
  kaTimer = millis();
}
