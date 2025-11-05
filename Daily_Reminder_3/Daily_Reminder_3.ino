/*
 * M5Stack Core Ink - Daily Reminder
 *
 * Zeigt jeden 2. Tag ein Yes/No Icon mit Datum und Batteriestand an.
 * Schaltet um Mitternacht um und geht danach in Deep Sleep.
 *
 * Hardware: M5Stack Core Ink Development Kit (1.54'' E-Ink Display)
 *
 * Features:
 * - Zeitsynchronisation:
 *     - Erster Start: automatisch via Internet
 *     - Später: manuell via Seitentaste beim Einschalten (kurz gedrückt halten)
 * - Wechselndes Yes/No Icon jeden 2. Tag (oder Testmodus)
 * - Datums- und Batterieanzeige
 * - Deep Sleep zur Energieeinsparung
 *
 * WICHTIG: ESP32 Core 3.x + M5Unified + M5GFX
 */

#include <M5Unified.h>
#include <M5GFX.h>
#include <WiFi.h>
#include <time.h>
#include <Preferences.h>
#include <Wire.h>
#include "Credentials.h"   // enthält ssid / password
#include "icons.h"         // enthält yes_icon_64x64 / no_icon_64x64

// Test-Modus: Definieren für Test (Umschalten alle 2 Minuten)
// Auskommentieren für Produktivbetrieb (Umschalten um Mitternacht / alle 2 Tage)
#define Test

// Zeitzone Zürich (CET: UTC+1, CEST: UTC+2 mit Sommerzeit)
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;        // UTC+1
const int daylightOffset_sec = 3600;    // +1 Stunde für Sommerzeit

// BM8563 RTC I2C Adresse
#define BM8563_I2C_ADDR 0x51

// Starttag für die Berechnung (wird beim ersten Start gesetzt)
#define PREF_NAMESPACE "dailyreminder"
#define PREF_START_DAY "startday"

// Display und Canvas
M5GFX display;
M5Canvas canvas(&display);

// RTC Datenstrukturen
struct RTCTime {
  uint8_t Hours;
  uint8_t Minutes;
  uint8_t Seconds;
};

struct RTCDate {
  uint16_t Year;
  uint8_t Month;
  uint8_t Date;
};

RTCTime rtcTime;
RTCDate rtcDate;

// Funktionsdeklarationen
void initDisplay();
void initRTC();
void getRTCTime(RTCTime* time);
void getRTCDate(RTCDate* date);
void setRTCTime(RTCTime* time);
void setRTCDate(RTCDate* date);
uint8_t bcd2ToByte(uint8_t value);
uint8_t byteToBcd2(uint8_t value);
void updateDisplay();
bool shouldShowYes();
int calculateDayOfYear(int year, int month, int day);
bool isLeapYear(int year);
float getBatteryVoltage();
int getBatteryPercent(float voltage);
void calculateAndEnterDeepSleep();
bool syncTimeFromInternet(bool setStartDay);

void setup() {
  Serial.begin(115200);
  delay(100);

  // M5Unified initialisieren
  auto cfg = M5.config();
  M5.begin(cfg);

  Serial.println("M5Stack Core Ink - Daily Reminder");
  Serial.println("==================================");

  // I2C initialisieren (CoreInk: SDA=21, SCL=22)
  Wire.begin(21, 22);

  // Display initialisieren
  initDisplay();

  // RTC initialisieren
  initRTC();

  // Starttag aus Preferences lesen
  Preferences preferences;
  preferences.begin(PREF_NAMESPACE, false);
  int startDay = preferences.getInt(PREF_START_DAY, -1);
  preferences.end();

  bool needInitialSync = (startDay == -1);

  // Seitenknopf beim Einschalten gedrückt halten => manueller Zeitsync
  bool manualSync = false;
  unsigned long t0 = millis();
  while (millis() - t0 < 1500) {  // ~1,5 Sekunden Beobachtungsfenster
    M5.update();
    if (M5.BtnPWR.wasPressed() || M5.BtnPWR.isPressed()) {
      manualSync = true;
    }
    delay(10);
  }

  if (needInitialSync) {
    Serial.println("Erster Start / Starttag fehlt -> Internet-Zeitsync...");
    syncTimeFromInternet(true);   // setzt Starttag in Preferences
  } else {
    Serial.printf("Starttag aus Speicher: Tag %d des Jahres\n", startDay);
    if (manualSync) {
      Serial.println("Manueller Zeitsync über Seitentaste angefordert...");
      syncTimeFromInternet(false);  // Starttag bleibt erhalten
    }
  }

  // Aktuelle Zeit von RTC lesen (nach evtl. Sync)
  getRTCTime(&rtcTime);
  getRTCDate(&rtcDate);

  Serial.printf("Aktuelle Zeit: %02d:%02d:%02d\n", rtcTime.Hours, rtcTime.Minutes, rtcTime.Seconds);
  Serial.printf("Datum: %02d.%02d.%04d\n", rtcDate.Date, rtcDate.Month, rtcDate.Year);

  // Display aktualisieren
  updateDisplay();

  // Deep Sleep berechnen und über M5.Power schlafen gehen
  calculateAndEnterDeepSleep();
}

void loop() {
  // Wird nie erreicht, da wir in Deep Sleep gehen
}

// ----------------------------------------------------------
// Display / RTC / Zeitfunktionen
// ----------------------------------------------------------

void initDisplay() {
  display.init();
  display.setRotation(0);
  display.setColorDepth(1); // 1-Bit für E-Ink

  canvas.setColorDepth(1);
  canvas.createSprite(200, 200);  // CoreInk: 200x200
  canvas.setTextDatum(top_left);

  Serial.println("Display initialisiert");
}

void initRTC() {
  Wire.beginTransmission(BM8563_I2C_ADDR);
  if (Wire.endTransmission() == 0) {
    Serial.println("RTC (BM8563) gefunden");
  } else {
    Serial.println("RTC (BM8563) nicht gefunden!");
  }
}

void getRTCTime(RTCTime* time) {
  Wire.beginTransmission(BM8563_I2C_ADDR);
  Wire.write(0x02); // Time register start
  Wire.endTransmission();

  Wire.requestFrom(BM8563_I2C_ADDR, 3);
  if (Wire.available() >= 3) {
    uint8_t sec  = Wire.read();
    uint8_t min  = Wire.read();
    uint8_t hour = Wire.read();

    time->Seconds = bcd2ToByte(sec & 0x7F);
    time->Minutes = bcd2ToByte(min & 0x7F);
    time->Hours   = bcd2ToByte(hour & 0x3F);
  }
}

void getRTCDate(RTCDate* date) {
  Wire.beginTransmission(BM8563_I2C_ADDR);
  Wire.write(0x05); // Date register start
  Wire.endTransmission();

  Wire.requestFrom(BM8563_I2C_ADDR, 4);
  if (Wire.available() >= 4) {
    uint8_t day   = Wire.read();
    Wire.read();          // Weekday - skip
    uint8_t month = Wire.read();
    uint8_t year  = Wire.read();

    date->Date  = bcd2ToByte(day & 0x3F);
    date->Month = bcd2ToByte(month & 0x1F);
    date->Year  = bcd2ToByte(year) + 2000;
  }
}

void setRTCTime(RTCTime* time) {
  Wire.beginTransmission(BM8563_I2C_ADDR);
  Wire.write(0x02); // Time register start
  Wire.write(byteToBcd2(time->Seconds));
  Wire.write(byteToBcd2(time->Minutes));
  Wire.write(byteToBcd2(time->Hours));
  Wire.endTransmission();
}

void setRTCDate(RTCDate* date) {
  Wire.beginTransmission(BM8563_I2C_ADDR);
  Wire.write(0x05); // Date register start
  Wire.write(byteToBcd2(date->Date));
  Wire.write(0x00); // Weekday
  Wire.write(byteToBcd2(date->Month));
  Wire.write(byteToBcd2(date->Year - 2000));
  Wire.endTransmission();
}

uint8_t bcd2ToByte(uint8_t value) {
  return (value >> 4) * 10 + (value & 0x0F);
}

uint8_t byteToBcd2(uint8_t value) {
  return ((value / 10) << 4) | (value % 10);
}

// ----------------------------------------------------------
// Internet-Zeitsynchronisation
// ----------------------------------------------------------

bool syncTimeFromInternet(bool setStartDay) {
  Serial.println("Starte Zeitsynchronisation via WiFi...");

  WiFi.begin(ssid, password);
  Serial.print("Verbinde mit WiFi");

  int wifiTimeout = 0;
  while (WiFi.status() != WL_CONNECTED && wifiTimeout < 20) {
    delay(500);
    Serial.print(".");
    wifiTimeout++;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nWiFi-Verbindung fehlgeschlagen!");
    return false;
  }

  Serial.println("\nWiFi verbunden!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  // Zeit vom NTP-Server holen
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Fehler beim Abrufen der Zeit!");
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    return false;
  }

  Serial.println("Zeit erfolgreich synchronisiert:");
  Serial.println(&timeinfo, "%A, %d.%m.%Y %H:%M:%S");

  // Zeit in RTC schreiben
  rtcTime.Hours   = timeinfo.tm_hour;
  rtcTime.Minutes = timeinfo.tm_min;
  rtcTime.Seconds = timeinfo.tm_sec;
  setRTCTime(&rtcTime);

  rtcDate.Year  = timeinfo.tm_year + 1900;
  rtcDate.Month = timeinfo.tm_mon + 1;
  rtcDate.Date  = timeinfo.tm_mday;
  setRTCDate(&rtcDate);

  // Starttag nur beim allerersten Mal setzen
  if (setStartDay) {
    int dayOfYear = timeinfo.tm_yday;   // 0..365
    Preferences preferences;
    preferences.begin(PREF_NAMESPACE, false);
    preferences.putInt(PREF_START_DAY, dayOfYear);
    preferences.end();
    Serial.printf("Starttag gespeichert: Tag %d des Jahres\n", dayOfYear);
  }

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  return true;
}

// ----------------------------------------------------------
// Anzeige / Logik
// ----------------------------------------------------------

void updateDisplay() {
  // Canvas löschen (weiß)
  canvas.fillSprite(TFT_WHITE);
  canvas.setTextColor(TFT_BLACK);
  canvas.setTextDatum(middle_center); // Zentrierte Textausrichtung

  // Datum - SEHR GROSS oben zentriert
  canvas.setFont(&fonts::Font6);
  char dateStr[20];
  sprintf(dateStr, "%02d.%02d.%02d", rtcDate.Date, rtcDate.Month, rtcDate.Year % 100); // Kurzformat: 05.11.25
  canvas.drawString(dateStr, 100, 25); // X=100 ist Mitte, Y=25

  // Uhrzeit - groß darunter zentriert
  canvas.setFont(&fonts::Font4);
  char timeStr[10];
  sprintf(timeStr, "%02d:%02d", rtcTime.Hours, rtcTime.Minutes);
  canvas.drawString(timeStr, 100, 58); // X=100 ist Mitte, Y=58

  // Yes/No Icon anzeigen (basierend auf Tag oder Minute im Test-Modus)
  bool showYes = shouldShowYes();

  // Icon zentriert anzeigen (64x64 Pixel Icon)
  int iconX = (200 - 64) / 2;
  int iconY = 75;

  if (showYes) {
    canvas.drawBitmap(iconX, iconY, yes_icon_64x64, 64, 64, TFT_BLACK);
    // "YES" Text unter dem Icon - sehr groß und zentriert
    canvas.setFont(&fonts::Font6);
    canvas.drawString("YES", 100, 148);
    Serial.println("Zeige YES Icon");
  } else {
    canvas.drawBitmap(iconX, iconY, no_icon_64x64, 64, 64, TFT_BLACK);
    // "NO" Text unter dem Icon - sehr groß und zentriert
    canvas.setFont(&fonts::Font6);
    canvas.drawString("NO", 100, 148);
    Serial.println("Zeige NO Icon");
  }

  // Batteriestand - GROSS unten zentriert
  float batteryVoltage = getBatteryVoltage();
  int batteryPercent = getBatteryPercent(batteryVoltage);
  char batteryStr[20];
  sprintf(batteryStr, "%d%% %.1fV", batteryPercent, batteryVoltage);
  canvas.setFont(&fonts::Font4);
  canvas.drawString(batteryStr, 100, 182); // X=100 ist Mitte, Y=182

  Serial.printf("Batteriestand: %d%% (%.2fV)\n", batteryPercent, batteryVoltage);

  // Canvas auf Display pushen
  canvas.pushSprite(0, 0);

  Serial.println("Display aktualisiert");
}

bool shouldShowYes() {
#ifdef Test
  // Test-Modus: Basierend auf aktueller Minute wechseln
  // Gerade Minuten (0,2,4,...) = YES, ungerade = NO
  bool isEvenMinute = (rtcTime.Minutes % 2) == 0;
  Serial.printf("Test-Modus: Minute %d -> %s\n", rtcTime.Minutes, isEvenMinute ? "YES" : "NO");
  return isEvenMinute;
#else
  // Produktiv-Modus: Basierend auf Tagen seit Start wechseln
  // Starttag aus Preferences lesen
  Preferences preferences;
  preferences.begin(PREF_NAMESPACE, true);
  int startDay = preferences.getInt(PREF_START_DAY, 0);
  preferences.end();

  // Aktuellen Tag des Jahres berechnen
  int currentDayOfYear = calculateDayOfYear(rtcDate.Year, rtcDate.Month, rtcDate.Date);

  // Differenz berechnen
  int daysSinceStart = currentDayOfYear - startDay;

  // Bei negativer Differenz (Jahreswechsel) korrigieren
  if (daysSinceStart < 0) {
    int daysInPrevYear = isLeapYear(rtcDate.Year - 1) ? 366 : 365;
    daysSinceStart += daysInPrevYear;
  }

  Serial.printf("Tage seit Start: %d\n", daysSinceStart);

  // Jeden 2. Tag wechseln: Tag 0,2,4,6... = YES, Tag 1,3,5,7... = NO
  return (daysSinceStart % 2) == 0;
#endif
}

int calculateDayOfYear(int year, int month, int day) {
  int daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

  if (isLeapYear(year)) {
    daysInMonth[1] = 29;
  }

  int dayOfYear = 0;
  for (int i = 0; i < month - 1; i++) {
    dayOfYear += daysInMonth[i];
  }
  dayOfYear += day;

  return dayOfYear;
}

bool isLeapYear(int year) {
  return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

// ----------------------------------------------------------
// Batterie
// ----------------------------------------------------------

float getBatteryVoltage() {
  // M5.Power.getBatteryVoltage() liefert mV
  int mv = M5.Power.getBatteryVoltage();
  float v = mv / 1000.0f;
  Serial.printf("Battery raw: %d mV (%.2f V)\n", mv, v);
  return v;
}

int getBatteryPercent(float voltage) {
  // Bevorzugt den vom Board berechneten Prozentwert
  int lvl = M5.Power.getBatteryLevel();  // 0..100 oder -1
  if (lvl >= 0) {
    Serial.printf("Battery Level (Board): %d%%\n", lvl);
    return lvl;
  }

  // Fallback: einfache Spannungs-basierte Schätzung
  if (voltage >= 4.1f) return 100;
  if (voltage <= 3.3f) return 0;
  int percent = (int)((voltage - 3.3f) / (4.1f - 3.3f) * 100.0f);
  percent = constrain(percent, 0, 100);
  Serial.printf("Battery Level (Fallback): %d%%\n", percent);
  return percent;
}

// ----------------------------------------------------------
// Sleep
// ----------------------------------------------------------

void calculateAndEnterDeepSleep() {
  int sleepSeconds;

#ifdef Test
  // Test-Modus: Wecken zur nächsten geraden Minute
  Serial.println("TEST-MODUS aktiv");

  int currentMinute = rtcTime.Minutes;
  int currentSecond = rtcTime.Seconds;

  // Nächste gerade Minute finden
  int nextEvenMinute = currentMinute;
  if (currentMinute % 2 != 0) {
    nextEvenMinute = currentMinute + 1;
  } else {
    nextEvenMinute = currentMinute + 2;
  }

  int minutesUntilWake = nextEvenMinute - currentMinute;
  if (minutesUntilWake <= 0) minutesUntilWake += 2;

  sleepSeconds = (minutesUntilWake * 60) - currentSecond;

  Serial.printf("Aktuelle Minute: %d, Nächste gerade Minute: %d\n", currentMinute, nextEvenMinute);
  Serial.printf("Schlafe für %d Sekunden (%d Minuten)\n", sleepSeconds, sleepSeconds / 60);

#else
  // Produktiv-Modus: Wecken um Mitternacht
  Serial.println("PRODUKTIV-MODUS aktiv");

  int currentHour   = rtcTime.Hours;
  int currentMinute = rtcTime.Minutes;
  int currentSecond = rtcTime.Seconds;

  int secondsSinceMidnight = currentHour * 3600 + currentMinute * 60 + currentSecond;
  int secondsUntilMidnight = 86400 - secondsSinceMidnight; // 86400 = 24 * 60 * 60

  sleepSeconds = secondsUntilMidnight;

  Serial.printf("Sekunden bis Mitternacht: %d (%d Stunden)\n", sleepSeconds, sleepSeconds / 3600);
#endif

  Serial.printf("Gehe in Deep Sleep (timerSleep) für %d Sekunden...\n", sleepSeconds);
  Serial.println("==================================");
  delay(200); // Kurze Verzögerung für Serial-Ausgabe

  // M5Unified macht den Sleep inkl. Power-Hold/RTC-Wakeup
  M5.Power.timerSleep(sleepSeconds);

  // Nach Wakeup läuft wieder setup()
}
