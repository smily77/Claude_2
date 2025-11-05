/*
 * M5Stack Core Ink - Daily Reminder
 *
 * Zeigt jeden 2. Tag ein Yes/No Icon mit Datum und Batteriestand an.
 * Schaltet um Mitternacht um und geht danach in Deep Sleep.
 *
 * Hardware: M5Stack Core Ink Development Kit (1.54'' E-Ink Display)
 *
 * Features:
 * - Zeitsynchronisation beim ersten Start (Zeitzone Zürich)
 * - Wechselndes Yes/No Icon jeden 2. Tag
 * - Datums- und Batterieanzeige
 * - Deep Sleep zur Energieeinsparung
 * - Test-Modus für schnelleres Testen
 */

#include <M5CoreInk.h>
#include <WiFi.h>
#include <time.h>
#include "Credentials.h"
#include "icons.h"

// Test-Modus: Definieren für Test (Umschalten jede gerade Minute)
// Auskommentieren für Produktivbetrieb (Umschalten um Mitternacht)
#define Test

// Zeitzone Zürich (CET: UTC+1, CEST: UTC+2 mit Sommerzeit)
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;        // UTC+1
const int daylightOffset_sec = 3600;    // +1 Stunde für Sommerzeit

// RTC (Real Time Clock) Objekt
RTC_TimeTypeDef RTCtime;
RTC_DateTypeDef RTCdate;

// Starttag für die Berechnung (wird beim ersten Start gesetzt)
#define PREF_NAMESPACE "dailyreminder"
#define PREF_START_DAY "startday"

Ink_Sprite InkPageSprite(&M5.M5Ink);

void setup() {
  // M5CoreInk initialisieren
  M5.begin();

  if (!M5.M5Ink.isInit()) {
    Serial.println("E-Ink Display Initialisierung fehlgeschlagen!");
    while (1) delay(100);
  }

  Serial.println("M5Stack Core Ink - Daily Reminder");
  Serial.println("==================================");

  // Prüfen, ob dies der erste Start ist
  Preferences preferences;
  preferences.begin(PREF_NAMESPACE, false);
  int startDay = preferences.getInt(PREF_START_DAY, -1);

  bool firstRun = (startDay == -1);

  if (firstRun) {
    Serial.println("Erster Start - Synchronisiere Zeit vom Internet...");

    // WiFi verbinden
    WiFi.begin(ssid, password);
    Serial.print("Verbinde mit WiFi");

    int wifiTimeout = 0;
    while (WiFi.status() != WL_CONNECTED && wifiTimeout < 20) {
      delay(500);
      Serial.print(".");
      wifiTimeout++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nWiFi verbunden!");
      Serial.print("IP: ");
      Serial.println(WiFi.localIP());

      // Zeit vom NTP Server holen
      configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

      // Warten bis Zeit synchronisiert ist
      struct tm timeinfo;
      if (getLocalTime(&timeinfo)) {
        Serial.println("Zeit erfolgreich synchronisiert:");
        Serial.println(&timeinfo, "%A, %d.%m.%Y %H:%M:%S");

        // Zeit in RTC schreiben
        RTCtime.Hours = timeinfo.tm_hour;
        RTCtime.Minutes = timeinfo.tm_min;
        RTCtime.Seconds = timeinfo.tm_sec;
        M5.rtc.SetTime(&RTCtime);

        RTCdate.Year = timeinfo.tm_year + 1900;
        RTCdate.Month = timeinfo.tm_mon + 1;
        RTCdate.Date = timeinfo.tm_mday;
        M5.rtc.SetDate(&RTCdate);

        // Starttag speichern (Tag des Jahres)
        int dayOfYear = timeinfo.tm_yday;
        preferences.putInt(PREF_START_DAY, dayOfYear);
        Serial.printf("Starttag gespeichert: Tag %d des Jahres\n", dayOfYear);
      } else {
        Serial.println("Fehler beim Abrufen der Zeit!");
      }

      WiFi.disconnect(true);
      WiFi.mode(WIFI_OFF);
    } else {
      Serial.println("\nWiFi-Verbindung fehlgeschlagen!");
      Serial.println("Verwende Standardzeit...");
    }
  } else {
    Serial.printf("Starttag aus Speicher: Tag %d des Jahres\n", startDay);
  }

  preferences.end();

  // Aktuelle Zeit von RTC lesen
  M5.rtc.GetTime(&RTCtime);
  M5.rtc.GetDate(&RTCdate);

  Serial.printf("Aktuelle Zeit: %02d:%02d:%02d\n", RTCtime.Hours, RTCtime.Minutes, RTCtime.Seconds);
  Serial.printf("Datum: %02d.%02d.%04d\n", RTCdate.Date, RTCdate.Month, RTCdate.Year);

  // Display aktualisieren
  updateDisplay();

  // Deep Sleep berechnen
  calculateAndEnterDeepSleep();
}

void loop() {
  // Wird nie erreicht, da wir in Deep Sleep gehen
}

void updateDisplay() {
  // Sprite für das gesamte Display erstellen
  if (InkPageSprite.creatSprite(0, 0, 200, 200, true)) {

    // Hintergrund löschen
    InkPageSprite.clear(INK_SPRITE_COLOR_WHITE);

    // Titel
    InkPageSprite.drawString(10, 10, "Daily Reminder", &AsciiFont8x16);
    InkPageSprite.drawLine(10, 30, 190, 30, INK_SPRITE_COLOR_BLACK);

    // Datum anzeigen
    char dateStr[20];
    sprintf(dateStr, "%02d.%02d.%04d", RTCdate.Date, RTCdate.Month, RTCdate.Year);
    InkPageSprite.drawString(10, 40, dateStr, &AsciiFont8x16);

    // Uhrzeit anzeigen
    char timeStr[10];
    sprintf(timeStr, "%02d:%02d", RTCtime.Hours, RTCtime.Minutes);
    InkPageSprite.drawString(10, 60, timeStr, &AsciiFont8x16);

    // Yes/No Icon anzeigen (basierend auf Tag)
    bool showYes = shouldShowYes();

    // Icon zentriert anzeigen (64x64 Pixel Icon)
    int iconX = (200 - 64) / 2;
    int iconY = 90;

    if (showYes) {
      InkPageSprite.drawBitmap(iconX, iconY, 64, 64, yes_icon_64x64);
      Serial.println("Zeige YES Icon");
    } else {
      InkPageSprite.drawBitmap(iconX, iconY, 64, 64, no_icon_64x64);
      Serial.println("Zeige NO Icon");
    }

    // Batteriestand anzeigen
    float batteryVoltage = getBatteryVoltage();
    int batteryPercent = getBatteryPercent(batteryVoltage);
    char batteryStr[30];
    sprintf(batteryStr, "Akku: %d%% (%.2fV)", batteryPercent, batteryVoltage);
    InkPageSprite.drawString(10, 170, batteryStr, &AsciiFont8x16);

    Serial.printf("Batteriestand: %d%% (%.2fV)\n", batteryPercent, batteryVoltage);

    // Sprite auf Display pushen
    InkPageSprite.pushSprite();
  }
}

bool shouldShowYes() {
  // Starttag aus Preferences lesen
  Preferences preferences;
  preferences.begin(PREF_NAMESPACE, true);
  int startDay = preferences.getInt(PREF_START_DAY, 0);
  preferences.end();

  // Aktuellen Tag des Jahres berechnen
  int currentDayOfYear = calculateDayOfYear(RTCdate.Year, RTCdate.Month, RTCdate.Date);

  // Differenz berechnen
  int daysSinceStart = currentDayOfYear - startDay;

  // Bei negativer Differenz (Jahreswechsel) korrigieren
  if (daysSinceStart < 0) {
    int daysInPrevYear = isLeapYear(RTCdate.Year - 1) ? 366 : 365;
    daysSinceStart += daysInPrevYear;
  }

  Serial.printf("Tage seit Start: %d\n", daysSinceStart);

  // Jeden 2. Tag wechseln: Tag 0,2,4,6... = YES, Tag 1,3,5,7... = NO
  return (daysSinceStart % 2) == 0;
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

float getBatteryVoltage() {
  // M5CoreInk Batteriespannung auslesen
  // Der ADC-Pin für die Batterie ist GPIO35
  analogSetAttenuation(ADC_11db);
  int adcValue = analogRead(35);

  // Umrechnung: ADC-Wert zu Spannung
  // M5CoreInk hat einen Spannungsteiler (2:1)
  float voltage = (adcValue * 3.3 / 4095.0) * 2.0;

  return voltage;
}

int getBatteryPercent(float voltage) {
  // LiPo-Batterie: ~4.2V voll, ~3.3V leer
  if (voltage >= 4.1) return 100;
  if (voltage <= 3.3) return 0;

  // Linear interpolieren
  int percent = (int)((voltage - 3.3) / (4.1 - 3.3) * 100.0);
  return constrain(percent, 0, 100);
}

void calculateAndEnterDeepSleep() {
  int sleepSeconds;

#ifdef Test
  // Test-Modus: Wecken zur nächsten geraden Minute
  Serial.println("TEST-MODUS aktiv");

  int currentMinute = RTCtime.Minutes;
  int currentSecond = RTCtime.Seconds;

  // Nächste gerade Minute finden
  int nextEvenMinute = currentMinute;
  if (currentMinute % 2 != 0) {
    nextEvenMinute = currentMinute + 1;
  } else {
    nextEvenMinute = currentMinute + 2;
  }

  // Sekunden bis zur nächsten geraden Minute
  int minutesUntilWake = nextEvenMinute - currentMinute;
  if (minutesUntilWake <= 0) minutesUntilWake += 2;

  sleepSeconds = (minutesUntilWake * 60) - currentSecond;

  Serial.printf("Aktuelle Minute: %d, Nächste gerade Minute: %d\n", currentMinute, nextEvenMinute);
  Serial.printf("Schlafe für %d Sekunden (%d Minuten)\n", sleepSeconds, sleepSeconds / 60);

#else
  // Produktiv-Modus: Wecken um Mitternacht
  Serial.println("PRODUKTIV-MODUS aktiv");

  int currentHour = RTCtime.Hours;
  int currentMinute = RTCtime.Minutes;
  int currentSecond = RTCtime.Seconds;

  // Sekunden bis Mitternacht berechnen
  int secondsSinceMidnight = currentHour * 3600 + currentMinute * 60 + currentSecond;
  int secondsUntilMidnight = 86400 - secondsSinceMidnight; // 86400 = 24 * 60 * 60

  sleepSeconds = secondsUntilMidnight;

  Serial.printf("Sekunden bis Mitternacht: %d (%d Stunden)\n", sleepSeconds, sleepSeconds / 3600);
#endif

  // In Deep Sleep gehen
  Serial.printf("Gehe in Deep Sleep für %d Sekunden...\n", sleepSeconds);
  Serial.println("==================================");
  delay(100); // Kurze Verzögerung für Serial-Ausgabe

  // Deep Sleep aktivieren
  esp_sleep_enable_timer_wakeup(sleepSeconds * 1000000ULL); // Mikrosekunden
  M5.shutdown();
}
