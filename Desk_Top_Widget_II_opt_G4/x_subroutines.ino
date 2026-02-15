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

// Zeitsynchronisation via HTTP
// Holt die Zeit aus dem "Date:" Header einer HTTPS-Antwort.
// Verwendet api.frankfurter.app (gleicher Server wie Waehrungsabruf).
// NTP (UDP Port 123) wird von 4G-Providern blockiert, daher nur HTTP.
time_t getTimeHTTP() {
  const char* host = "api.frankfurter.app";

  if (DEBUG) Serial.println("HTTP time: connecting to frankfurter.app...");

  clientSec.stop();
  delay(200);

  clientSec.setInsecure();

  // TLS-Handshake ueber 4G kann langsam sein
  if (!clientSec.connect(host, 443)) {
    if (DEBUG) Serial.println("HTTP time: connection failed");
    return 0;
  }

  if (DEBUG) Serial.println("HTTP time: connected");

  clientSec.print(String("GET ") + "/latest" + " HTTP/1.1\r\n" +
                  "Host: " + host + "\r\n" +
                  "User-Agent: ESP8266Widget\r\n" +
                  "Connection: close\r\n\r\n");

  time_t result = 0;

  // Warte auf Antwort
  unsigned long httpStart = millis();
  while (clientSec.connected() && (millis() - httpStart < 15000)) {
    if (!clientSec.available()) {
      delay(50);
      continue;
    }
    String line = clientSec.readStringUntil('\n');

    if (line.startsWith("Date:") || line.startsWith("date:")) {
      // Format: "Date: Sat, 15 Feb 2026 10:30:00 GMT"
      int commaIdx = line.indexOf(',');
      if (commaIdx < 0) break;

      String datePart = line.substring(commaIdx + 2);
      datePart.trim();

      int d  = datePart.substring(0, 2).toInt();
      String monStr = datePart.substring(3, 6);
      int y  = datePart.substring(7, 11).toInt();
      int h  = datePart.substring(12, 14).toInt();
      int mi = datePart.substring(15, 17).toInt();
      int s  = datePart.substring(18, 20).toInt();

      const char* months[] = {"Jan","Feb","Mar","Apr","May","Jun",
                              "Jul","Aug","Sep","Oct","Nov","Dec"};
      int mon = 0;
      for (int i = 0; i < 12; i++) {
        if (monStr.equalsIgnoreCase(months[i])) {
          mon = i + 1;
          break;
        }
      }

      if (mon > 0 && y > 2020 && d > 0) {
        tmElements_t tm;
        tm.Year = y - 1970;
        tm.Month = mon;
        tm.Day = d;
        tm.Hour = h;
        tm.Minute = mi;
        tm.Second = s;

        time_t utcTime = makeTime(tm);
        result = utcTime + getTimezoneOffset(0, utcTime);

        if (DEBUG) {
          Serial.print("HTTP time: ");
          Serial.print(d); Serial.print("."); Serial.print(mon);
          Serial.print("."); Serial.print(y); Serial.print(" ");
          Serial.print(h); Serial.print(":"); Serial.print(mi);
          Serial.print(":"); Serial.println(s);
        }
      }
      break;
    }

    if (line == "\r") break;
  }

  clientSec.stop();
  return result;
}

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
