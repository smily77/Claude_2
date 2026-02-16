// Sucht Airport-Code in der Datenbank und gibt Timezone-Info zurueck
boolean lookupAirportTimezone(const char* code, TimezoneInfo &tz) {
  for (int i = 0; i < AIRPORT_DATABASE_SIZE; i++) {
    AirportTimezone apt;
    memcpy_P(&apt, &AIRPORT_DATABASE[i], sizeof(AirportTimezone));

    if (strcmp(code, apt.code) == 0) {
      tz.airCode = String(code);
      tz.stdOffset = apt.stdOffset;
      tz.dstOffset = apt.dstOffset;
      tz.dstType = apt.dstType;
      return true;
    }
  }
  return false;
}

// Initialisiert alle Timezones basierend auf Airport-Codes
void initializeTimezones() {
  if (DEBUG) Serial.println("Initializing timezones...");

  for (int i = 0; i < 7; i++) {
    if (lookupAirportTimezone(AIRPORT_CODES[i], timezones[i])) {
      if (DEBUG) {
        Serial.print(AIRPORT_CODES[i]);
        Serial.print(" -> UTC");
        Serial.print(timezones[i].stdOffset / 3600.0);
        Serial.print(" / UTC");
        Serial.print(timezones[i].dstOffset / 3600.0);
        Serial.print(" (DST Type: ");
        Serial.print(timezones[i].dstType);
        Serial.println(")");
      }

      if (firstRun) {
        tft.print(AIRPORT_CODES[i]);
        tft.print(": UTC");
        if (timezones[i].stdOffset >= 0) tft.print("+");
        tft.println(timezones[i].stdOffset / 3600.0);
      }
    } else {
      if (DEBUG) {
        Serial.print("ERROR: Airport code ");
        Serial.print(AIRPORT_CODES[i]);
        Serial.println(" not found in database!");
      }

      if (firstRun) {
        tft.setTextColor(ST7735_RED);
        tft.print("ERROR: ");
        tft.print(AIRPORT_CODES[i]);
        tft.println(" not found!");
        tft.setTextColor(ST7735_WHITE);
      }

      // Fallback: UTC
      timezones[i].airCode = String(AIRPORT_CODES[i]);
      timezones[i].stdOffset = 0;
      timezones[i].dstOffset = 0;
      timezones[i].dstType = 0;
    }
  }

  if (DEBUG) Serial.println("Timezones initialized.");
}

void readBMP(double &T, double &P) {
  status = pressure.startTemperature();
  if (status != 0) {
    delay(status);
    status = pressure.getTemperature(T);
    if (status != 0) {
      status = pressure.startPressure(3);
      if (status != 0) {
        delay(status);
        status = pressure.getPressure(P, T);
      }
    }
  }
}

// ============================================
// NTP ZEITSYNCHRONISATION VIA A7670E
// ============================================

// NTP-Server konfigurieren und Zeit synchronisieren
// Gibt lokale Zeit (mit Timezone-Offset) zurueck, oder 0 bei Fehler
time_t getNtpTimeA7670E() {
  if (DEBUG) Serial.println("NTP via A7670E...");

  // NTP-Server konfigurieren (Offset 0 = UTC)
  if (!sendATCommand("AT+CNTP=\"pool.ntp.org\",0", "OK", 5000)) {
    if (DEBUG) Serial.println("NTP: config failed");
    return 0;
  }

  // NTP-Sync ausfuehren
  String ntpResp;
  if (!sendATCommand("AT+CNTP", "+CNTP:", 15000, &ntpResp)) {
    if (DEBUG) Serial.println("NTP: sync failed");
    return 0;
  }

  // Pruefen ob Sync erfolgreich (+CNTP: 0 = OK)
  if (ntpResp.indexOf("+CNTP: 0") < 0) {
    if (DEBUG) {
      Serial.print("NTP: sync error: ");
      Serial.println(ntpResp);
    }
    return 0;
  }

  // Zeit auslesen
  String cclkResp;
  if (!sendATCommand("AT+CCLK?", "+CCLK:", 5000, &cclkResp)) {
    if (DEBUG) Serial.println("NTP: read time failed");
    return 0;
  }

  return parseCCLKResponse(cclkResp);
}

// Parst die +CCLK Antwort in eine time_t
// Format: +CCLK: "YY/MM/DD,HH:MM:SS+ZZ" oder +CCLK: "YY/MM/DD,HH:MM:SS-ZZ"
// ZZ = Zeitzone in Viertelstunden von UTC
time_t parseCCLKResponse(String cclkResp) {
  // Suche den Anfang des Zeitstrings
  int quoteStart = cclkResp.indexOf('"');
  int quoteEnd = cclkResp.indexOf('"', quoteStart + 1);

  if (quoteStart < 0 || quoteEnd < 0 || quoteEnd - quoteStart < 17) {
    if (DEBUG) Serial.println("CCLK: parse error");
    return 0;
  }

  String timeStr = cclkResp.substring(quoteStart + 1, quoteEnd);
  // timeStr z.B. "26/02/16,14:30:00+00"

  int y  = timeStr.substring(0, 2).toInt() + 2000;
  int mo = timeStr.substring(3, 5).toInt();
  int d  = timeStr.substring(6, 8).toInt();
  int h  = timeStr.substring(9, 11).toInt();
  int mi = timeStr.substring(12, 14).toInt();
  int s  = timeStr.substring(15, 17).toInt();

  // Zeitzone parsen (Viertelstunden)
  int tzOffset = 0;
  if (timeStr.length() >= 19) {
    char tzSign = timeStr.charAt(17);
    int tzQuarters = timeStr.substring(18).toInt();
    tzOffset = tzQuarters * 15 * 60;  // In Sekunden umrechnen
    if (tzSign == '-') tzOffset = -tzOffset;
  }

  if (y < 2024 || mo < 1 || mo > 12 || d < 1 || d > 31) {
    if (DEBUG) Serial.println("CCLK: invalid date");
    return 0;
  }

  tmElements_t tm;
  tm.Year = y - 1970;
  tm.Month = mo;
  tm.Day = d;
  tm.Hour = h;
  tm.Minute = mi;
  tm.Second = s;

  time_t moduleTime = makeTime(tm);

  // moduleTime ist die Modul-Zeit mit dem von CNTP gesetzten Offset
  // Wir haben CNTP mit Offset 0 konfiguriert, also ist es UTC
  // Aber der CCLK-Zeitzonenoffset (tzOffset) zeigt was das Modul verwendet
  // Wir rechnen zurueck auf UTC und dann auf lokale Zeit
  time_t utcTime = moduleTime - tzOffset;

  // Lokale Zeit berechnen (Zeitzone 0 = Zuerich)
  time_t localTime = utcTime + getTimezoneOffset(0, utcTime);

  if (DEBUG) {
    Serial.print("CCLK parsed: ");
    Serial.print(y); Serial.print("/"); Serial.print(mo); Serial.print("/"); Serial.print(d);
    Serial.print(" "); Serial.print(h); Serial.print(":"); Serial.print(mi);
    Serial.print(":"); Serial.println(s);
    Serial.print("UTC: "); Serial.println(utcTime);
    Serial.print("Local: "); Serial.println(localTime);
  }

  return localTime;
}

// HTTP-Fallback Zeitsynchronisation via A7670E
// Holt die Zeit aus dem "Date:" Header einer HTTPS-Antwort
time_t getTimeHTTP_A7670E() {
  if (DEBUG) Serial.println("HTTP time via A7670E...");

  // HTTP initialisieren
  sendATCommand("AT+HTTPTERM", "OK", 3000);  // Falls noch offen
  delay(500);

  if (!sendATCommand("AT+HTTPINIT", "OK", 5000)) {
    if (DEBUG) Serial.println("HTTP time: HTTPINIT failed");
    return 0;
  }

  // SSL aktivieren
  sendATCommand("AT+HTTPPARA=\"SSLCFG\",1", "OK", 3000);

  // URL setzen
  if (!sendATCommand("AT+HTTPPARA=\"URL\",\"https://api.frankfurter.app/latest\"", "OK", 5000)) {
    sendATCommand("AT+HTTPTERM", "OK", 3000);
    return 0;
  }

  // GET ausfuehren
  String httpResp;
  if (!sendATCommand("AT+HTTPACTION=0", "+HTTPACTION:", 20000, &httpResp)) {
    sendATCommand("AT+HTTPTERM", "OK", 3000);
    return 0;
  }

  // Response Header lesen (fuer Date:-Header)
  // Bei A7670E HTTP-Stack bekommen wir den Date-Header nicht direkt.
  // Wir lesen den Body und parsen das "date" Feld aus der JSON-Antwort.
  // frankfurter.app gibt ein "date":"YYYY-MM-DD" Feld zurueck.

  // Datenlänge aus +HTTPACTION extrahieren
  int commaIdx = httpResp.indexOf(",200,");
  if (commaIdx < 0) {
    // HTTP-Status war nicht 200
    sendATCommand("AT+HTTPTERM", "OK", 3000);
    return 0;
  }
  int dataLen = httpResp.substring(commaIdx + 5).toInt();
  if (dataLen <= 0 || dataLen > 2048) {
    sendATCommand("AT+HTTPTERM", "OK", 3000);
    return 0;
  }

  // Body lesen
  char readCmd[32];
  snprintf(readCmd, sizeof(readCmd), "AT+HTTPREAD=0,%d", dataLen);
  String bodyResp;
  if (!sendATCommand(readCmd, "OK", 10000, &bodyResp)) {
    sendATCommand("AT+HTTPTERM", "OK", 3000);
    return 0;
  }

  sendATCommand("AT+HTTPTERM", "OK", 3000);

  // "date":"2026-02-16" aus JSON-Body parsen
  int dateIdx = bodyResp.indexOf("\"date\":\"");
  if (dateIdx < 0) {
    if (DEBUG) Serial.println("HTTP time: no date in response");
    return 0;
  }

  String dateStr = bodyResp.substring(dateIdx + 8, dateIdx + 18);
  // dateStr z.B. "2026-02-16"

  int y  = dateStr.substring(0, 4).toInt();
  int mo = dateStr.substring(5, 7).toInt();
  int d  = dateStr.substring(8, 10).toInt();

  if (y < 2024 || mo < 1 || mo > 12 || d < 1 || d > 31) {
    if (DEBUG) Serial.println("HTTP time: invalid date");
    return 0;
  }

  // Wir haben nur das Datum, keine genaue Uhrzeit
  // Setze auf 12:00 UTC als Naeherung
  tmElements_t tm;
  tm.Year = y - 1970;
  tm.Month = mo;
  tm.Day = d;
  tm.Hour = 12;
  tm.Minute = 0;
  tm.Second = 0;

  time_t utcTime = makeTime(tm);
  time_t localTime = utcTime + getTimezoneOffset(0, utcTime);

  if (DEBUG) {
    Serial.print("HTTP date: ");
    Serial.println(dateStr);
    Serial.print("Local time (approx): ");
    Serial.println(localTime);
  }

  return localTime;
}

// ============================================
// DST-BERECHNUNG (unveraendert aus G4-Version)
// ============================================

// Berechnet ob DST aktiv ist fuer EU-Zeitzonen
boolean isDstEU(time_t t) {
  int y = year(t);
  int m = month(t);
  int d = day(t);
  int h = hour(t);

  int marchLastSunday = 31 - ((5 * y / 4 + 4) % 7);
  int octoberLastSunday = 31 - ((5 * y / 4 + 1) % 7);

  if (m < 3 || m > 10) return false;
  if (m > 3 && m < 10) return true;

  if (m == 3) {
    if (d < marchLastSunday) return false;
    if (d > marchLastSunday) return true;
    return (h >= 2);
  }

  if (m == 10) {
    if (d < octoberLastSunday) return true;
    if (d > octoberLastSunday) return false;
    return (h < 3);
  }

  return false;
}

// Berechnet ob DST aktiv ist fuer US-Zeitzonen
boolean isDstUS(time_t t) {
  int y = year(t);
  int m = month(t);
  int d = day(t);

  if (m < 3 || m > 11) return false;
  if (m > 3 && m < 11) return true;

  int marchFirstSunday = (14 - (5 * y / 4 + 1) % 7);
  int marchSecondSunday = marchFirstSunday + 7;

  int novemberFirstSunday = (7 - (5 * y / 4 + 1) % 7);
  if (novemberFirstSunday == 0) novemberFirstSunday = 7;

  if (m == 3) {
    return (d >= marchSecondSunday);
  }

  if (m == 11) {
    return (d < novemberFirstSunday);
  }

  return false;
}

// Berechnet ob DST aktiv ist fuer Australien
boolean isDstAU(time_t t) {
  int y = year(t);
  int m = month(t);
  int d = day(t);

  if (m < 4 || m > 9) return true;
  if (m > 4 && m < 10) return false;

  int octoberFirstSunday = (7 - (5 * y / 4 + 1) % 7);
  if (octoberFirstSunday == 0) octoberFirstSunday = 7;

  int aprilFirstSunday = (7 - (5 * y / 4 + 4) % 7);
  if (aprilFirstSunday == 0) aprilFirstSunday = 7;

  if (m == 10) {
    return (d >= octoberFirstSunday);
  }

  if (m == 4) {
    return (d < aprilFirstSunday);
  }

  return false;
}

// Berechnet ob DST aktiv ist fuer Neuseeland
boolean isDstNZ(time_t t) {
  int y = year(t);
  int m = month(t);
  int d = day(t);

  if (m < 4 || m > 9) return true;
  if (m > 4 && m < 9) return false;

  int septemberLastSunday = 30 - ((5 * y / 4 + 2) % 7);

  int aprilFirstSunday = (7 - (5 * y / 4 + 4) % 7);
  if (aprilFirstSunday == 0) aprilFirstSunday = 7;

  if (m == 9) {
    return (d >= septemberLastSunday);
  }

  if (m == 4) {
    return (d < aprilFirstSunday);
  }

  return false;
}

// Berechnet den Timezone-Offset
int getTimezoneOffset(int tzIndex, time_t t) {
  TimezoneInfo tz = timezones[tzIndex];

  if (tz.dstType == 0) {
    return tz.stdOffset;
  }

  boolean isDst = false;
  switch (tz.dstType) {
    case 1: isDst = isDstEU(t); break;
    case 2: isDst = isDstUS(t); break;
    case 3: isDst = isDstAU(t); break;
    case 4: isDst = isDstNZ(t); break;
  }

  return isDst ? tz.dstOffset : tz.stdOffset;
}

// Berechnet die Zeit fuer eine bestimmte Zeitzone
time_t getTimeForTimezone(int tzIndex, time_t localTime) {
  int localOffset = getTimezoneOffset(0, localTime);
  time_t utcTime = localTime - localOffset;
  int targetOffset = getTimezoneOffset(tzIndex, utcTime);
  return utcTime + targetOffset;
}
