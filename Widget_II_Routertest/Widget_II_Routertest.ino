// Widget_II_Routertest
// Testprogramm: Periodisches WiFi Connect/Disconnect um 4G-Hotspot aktiv zu halten
//
// Hardware: ESP8266 + Adafruit ST7735 1.8" TFT Display
// Verdrahtung identisch zu Desk_Top_Widget_II_opt:
//   TFT_CS  = GPIO 15
//   TFT_DC  = GPIO 2
//   TFT_RST = GPIO 12
//   LED     = GPIO 4 (Hintergrundbeleuchtung)
//   LDR     = A0 (Helligkeitssensor)
//
// Funktion:
//   - Verbindet sich periodisch mit dem WLAN (ssid2) und trennt wieder
//   - Zeigt Countdown bis zum naechsten Verbindungsversuch (MM:SS)
//   - Hintergrund GRUEN wenn Verbindung erfolgreich
//   - Hintergrund ROT wenn Verbindung fehlgeschlagen
//
// Zweck:
//   Herausfinden ob periodisches Verbinden den 4G-Hotspot aktiv haelt
//   und welche Periodizitaet dafuer noetig ist.

#include <ESP8266WiFi.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Credentials.h>

// ============================================
// KONFIGURATION
// ============================================
// Intervall zwischen WiFi-Verbindungsversuchen in Sekunden
// Auf 15 Minuten (900 Sekunden) eingestellt
// Zum Testen kann man den Wert verkleinern, z.B. 60 fuer 1 Minute
const unsigned long CONNECT_INTERVAL_SEC = 5 * 60;  // 15 Minuten

// Maximale Wartezeit fuer WiFi-Verbindung in Sekunden
const int WIFI_CONNECT_TIMEOUT_SEC = 30;
// ============================================

// TFT Pin-Belegung (identisch zu Desk_Top_Widget_II_opt)
#define TFT_PIN_CS   15
#define TFT_PIN_DC   2
#define TFT_PIN_RST  12
#define LED_PIN      4

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_PIN_CS, TFT_PIN_DC, TFT_PIN_RST);

// Status
enum ConnectStatus {
  STATUS_INIT,
  STATUS_OK,
  STATUS_FAIL
};

ConnectStatus lastStatus = STATUS_INIT;
unsigned long countdownRemaining = 0;  // Sekunden bis zum naechsten Connect
unsigned long lastSecondMillis = 0;
int lastDisplayedMinutes = -1;
int lastDisplayedSeconds = -1;
int connectCount = 0;
int successCount = 0;
int failCount = 0;

void setup() {
  Serial.begin(9600);
  Serial.println();
  Serial.println("Widget_II_Routertest");
  Serial.println("====================");

  // LED / Hintergrundbeleuchtung
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // TFT initialisieren (identisch zu Desk_Top_Widget_II_opt)
  tft.initR(INITR_BLACKTAB);
  tft.setTextWrap(false);
  tft.setRotation(1);
  tft.fillScreen(ST7735_BLACK);
  tft.setTextColor(ST7735_WHITE, ST7735_BLACK);
  tft.setFont();
  tft.setCursor(0, 0);

  // WiFi im Station-Modus, aber noch nicht verbunden
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  // Startbildschirm
  tft.println("Router Keep-Alive Test");
  tft.println();
  tft.print("SSID: ");
  tft.println(ssid2);
  tft.println();
  tft.print("Intervall: ");
  tft.print(CONNECT_INTERVAL_SEC / 60);
  tft.println(" Min");
  tft.println();
  tft.println("Erster Connect in 3s...");

  Serial.print("SSID: ");
  Serial.println(ssid2);
  Serial.print("Intervall: ");
  Serial.print(CONNECT_INTERVAL_SEC / 60);
  Serial.println(" Minuten");

  delay(3000);

  // Sofort ersten Verbindungsversuch starten
  doWiFiConnectTest();

  // Countdown fuer naechsten Versuch starten
  countdownRemaining = CONNECT_INTERVAL_SEC;
  lastSecondMillis = millis();
  displayCountdown(true);
}

void loop() {
  // Helligkeitssteuerung (wie in Desk_Top_Widget_II_opt)
  int helligkeit = analogRead(A0);
  delay(100);
  if (helligkeit > 1010) helligkeit = 1010;
  analogWrite(LED_PIN, helligkeit);

  // Jede Sekunde Countdown aktualisieren
  unsigned long now = millis();
  if (now - lastSecondMillis >= 1000) {
    lastSecondMillis += 1000;

    if (countdownRemaining > 0) {
      countdownRemaining--;
      displayCountdown(false);
    }

    // Countdown abgelaufen -> WiFi-Verbindungsversuch
    if (countdownRemaining == 0) {
      doWiFiConnectTest();
      countdownRemaining = CONNECT_INTERVAL_SEC;
      lastDisplayedMinutes = -1;  // Display-Update erzwingen
      lastDisplayedSeconds = -1;
      displayCountdown(true);
    }
  }
}

// Fuehrt einen WiFi Connect/Disconnect-Zyklus durch
void doWiFiConnectTest() {
  connectCount++;

  Serial.println();
  Serial.print("=== Verbindungsversuch #");
  Serial.print(connectCount);
  Serial.println(" ===");

  // Verbindungsversuch auf dem Display anzeigen
  tft.fillScreen(ST7735_BLACK);
  tft.setFont();
  tft.setTextColor(ST7735_WHITE, ST7735_BLACK);
  tft.setCursor(0, 0);
  tft.println("Verbinde mit WiFi...");
  tft.println();
  tft.print("SSID: ");
  tft.println(ssid2);
  tft.println();

  // WiFi verbinden
  WiFi.begin(ssid2, password2);

  int waitCount = 0;
  while (WiFi.status() != WL_CONNECTED && waitCount < WIFI_CONNECT_TIMEOUT_SEC * 2) {
    delay(500);
    tft.print(".");
    waitCount++;
  }

  tft.println();
  tft.println();

  if (WiFi.status() == WL_CONNECTED) {
    // Erfolg
    successCount++;
    lastStatus = STATUS_OK;

    Serial.print("Verbunden! IP: ");
    Serial.println(WiFi.localIP());

    tft.setTextColor(ST7735_WHITE);
    tft.println("VERBUNDEN!");
    tft.print("IP: ");
    tft.println(WiFi.localIP());

    // Kurz verbunden lassen damit der Router es registriert
    delay(2000);

    // Trennen
    WiFi.disconnect();
    delay(500);

    Serial.println("Getrennt.");
  } else {
    // Fehlgeschlagen
    failCount++;
    lastStatus = STATUS_FAIL;

    Serial.println("Verbindung fehlgeschlagen!");

    tft.setTextColor(ST7735_RED);
    tft.println("FEHLGESCHLAGEN!");

    WiFi.disconnect();
    delay(500);
  }

  Serial.print("Statistik: ");
  Serial.print(successCount);
  Serial.print(" OK / ");
  Serial.print(failCount);
  Serial.println(" FAIL");
}

// Zeigt den Countdown-Bildschirm an
// fullRedraw: kompletter Neuaufbau des Bildschirms
void displayCountdown(bool fullRedraw) {
  int minutes = countdownRemaining / 60;
  int seconds = countdownRemaining % 60;

  // Nur neu zeichnen wenn sich die Anzeige aendert
  if (!fullRedraw && minutes == lastDisplayedMinutes && seconds == lastDisplayedSeconds) {
    return;
  }

  if (fullRedraw) {
    // Hintergrundfarbe je nach letztem Status
    uint16_t bgColor;
    switch (lastStatus) {
      case STATUS_OK:
        bgColor = tft.color565(0, 100, 0);  // Dunkelgruen
        break;
      case STATUS_FAIL:
        bgColor = tft.color565(139, 0, 0);  // Dunkelrot
        break;
      default:
        bgColor = ST7735_BLACK;
        break;
    }
    tft.fillScreen(bgColor);

    // Header
    tft.setFont();
    tft.setTextColor(ST7735_WHITE);
    tft.setCursor(2, 2);
    tft.print("Router Keep-Alive Test");

    // Status-Text
    tft.setCursor(2, 16);
    switch (lastStatus) {
      case STATUS_OK:
        tft.setTextColor(ST7735_GREEN);
        tft.print("Letzter Connect: OK");
        break;
      case STATUS_FAIL:
        tft.setTextColor(ST7735_RED);
        tft.print("Letzter Connect: FAIL");
        break;
      default:
        tft.setTextColor(ST7735_YELLOW);
        tft.print("Warte auf ersten Test...");
        break;
    }

    // Statistik
    tft.setFont();
    tft.setTextColor(ST7735_WHITE);
    tft.setCursor(2, 100);
    tft.print("OK:");
    tft.print(successCount);
    tft.print(" FAIL:");
    tft.print(failCount);
    tft.print(" Total:");
    tft.print(connectCount);

    // SSID
    tft.setCursor(2, 116);
    tft.setTextColor(ST7735_YELLOW);
    tft.print("SSID: ");
    tft.print(ssid2);
  }

  // Countdown gross anzeigen (nur den Zahlenbereich updaten)
  // Bereich fuer die grosse Countdown-Anzeige loeschen
  uint16_t bgColor;
  switch (lastStatus) {
    case STATUS_OK:
      bgColor = tft.color565(0, 100, 0);
      break;
    case STATUS_FAIL:
      bgColor = tft.color565(139, 0, 0);
      break;
    default:
      bgColor = ST7735_BLACK;
      break;
  }

  // Nur den Countdown-Bereich neu zeichnen
  tft.fillRect(0, 35, 160, 55, bgColor);

  // "Naechster Connect:" Label
  tft.setFont();
  tft.setTextColor(ST7735_WHITE);
  tft.setCursor(20, 38);
  tft.print("Naechster Connect:");

  // Grosse Countdown-Anzeige
  tft.setFont(&FreeSansBold18pt7b);
  tft.setTextColor(ST7735_WHITE);

  char buf[8];
  sprintf(buf, "%02d:%02d", minutes, seconds);

  // Zentriert anzeigen (ca. 5 Zeichen breit bei dieser Font)
  tft.setCursor(22, 82);
  tft.print(buf);

  lastDisplayedMinutes = minutes;
  lastDisplayedSeconds = seconds;
}
