// Wechselkurs-Abruf via A7670E LTE-Modul (AT+HTTP Kommandos)
// Gibt true zurueck bei Erfolg, false bei Fehler

boolean catchCurrencies() {
  if (DEBUG) Serial.println("Fetching currencies via A7670E...");

  // Sicherstellen dass kein alter HTTP-Kontext offen ist
  sendATCommand("AT+HTTPTERM", "OK", 3000);
  delay(500);

  // HTTP initialisieren
  if (!sendATCommand("AT+HTTPINIT", "OK", 5000)) {
    if (DEBUG) Serial.println("Currencies: HTTPINIT failed");
    return false;
  }

  // SSL aktivieren
  sendATCommand("AT+HTTPPARA=\"SSLCFG\",1", "OK", 3000);

  // URL setzen
  if (!sendATCommand("AT+HTTPPARA=\"URL\",\"https://api.frankfurter.app/latest\"", "OK", 5000)) {
    if (DEBUG) Serial.println("Currencies: URL set failed");
    sendATCommand("AT+HTTPTERM", "OK", 3000);
    return false;
  }

  // GET-Request ausfuehren
  String httpResp;
  if (!sendATCommand("AT+HTTPACTION=0", "+HTTPACTION:", 20000, &httpResp)) {
    if (DEBUG) Serial.println("Currencies: HTTPACTION failed");
    sendATCommand("AT+HTTPTERM", "OK", 3000);
    return false;
  }

  // HTTP-Status und Datenlaenge pruefen
  // Antwort-Format: +HTTPACTION: 0,200,<datalen>
  int statusIdx = httpResp.indexOf("+HTTPACTION:");
  if (statusIdx < 0) {
    sendATCommand("AT+HTTPTERM", "OK", 3000);
    return false;
  }

  // Status-Code extrahieren
  String actionStr = httpResp.substring(statusIdx);
  int firstComma = actionStr.indexOf(',');
  int secondComma = actionStr.indexOf(',', firstComma + 1);

  if (firstComma < 0 || secondComma < 0) {
    sendATCommand("AT+HTTPTERM", "OK", 3000);
    return false;
  }

  int httpStatus = actionStr.substring(firstComma + 1, secondComma).toInt();
  int dataLen = actionStr.substring(secondComma + 1).toInt();

  if (DEBUG) {
    Serial.print("HTTP Status: ");
    Serial.print(httpStatus);
    Serial.print(", Length: ");
    Serial.println(dataLen);
  }

  if (httpStatus != 200 || dataLen <= 0 || dataLen > 2048) {
    if (DEBUG) Serial.println("Currencies: bad HTTP response");
    sendATCommand("AT+HTTPTERM", "OK", 3000);
    return false;
  }

  // Response Body lesen
  char readCmd[32];
  snprintf(readCmd, sizeof(readCmd), "AT+HTTPREAD=0,%d", dataLen);

  String bodyResp;
  if (!sendATCommand(readCmd, "OK", 10000, &bodyResp)) {
    if (DEBUG) Serial.println("Currencies: HTTPREAD failed");
    sendATCommand("AT+HTTPTERM", "OK", 3000);
    return false;
  }

  // HTTP beenden
  sendATCommand("AT+HTTPTERM", "OK", 3000);

  // Den eigentlichen JSON-Body aus der Antwort extrahieren
  // Die Antwort enthaelt: +HTTPREAD: <len>\r\n<body>\r\nOK
  String line = bodyResp;
  int bodyStart = line.indexOf("{");
  if (bodyStart >= 0) {
    line = line.substring(bodyStart);
    // Abschneiden nach dem letzten '}'
    int bodyEnd = line.lastIndexOf("}");
    if (bodyEnd >= 0) {
      line = line.substring(0, bodyEnd + 1);
    }
  }

  if (DEBUG) {
    Serial.println("JSON Body:");
    Serial.println(line);
  }

  // Pruefen ob Antwort gueltig
  if (line.length() < 10 || line.indexOf("rates") < 0) {
    if (DEBUG) Serial.println("Invalid response");
    return false;
  }

  // Parse Wechselkurse aus JSON (identisch zur G4-Version)
  String result;
  for (int i = 0; i < 4; i++) {
    if (fxSym[i].compareTo("EUR")) {
      result = line.substring(line.indexOf(fxSym[i]) + 5,
                              line.indexOf(",", line.indexOf(fxSym[i])));
      fxValue[i] = result.toFloat();
    }
    else {
      fxValue[i] = 1.00;
    }
    if (DEBUG) {
      Serial.print(fxSym[i]);
      Serial.print(":   ");
      Serial.println(fxValue[i]);
    }
  }

  // Berechne CHF zu anderen Waehrungen
  for (int l = 1; l < 4; l++) {
    fxValue[l] = fxValue[0] / fxValue[l];
    if (firstRun) {
      tft.print(fxSym[l]);
      tft.print(": ");
      tft.println(fxValue[l]);
    }
  }

  if (DEBUG) Serial.println("Currencies OK");
  return true;
}
