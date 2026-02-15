// Widget_II_Bright
// Zeigt den Analogwert des LDR (A0) und die daraus berechnete
// LED-Helligkeit auf dem TFT Display.
// Zur Kalibrierung der Helligkeitssteuerung.
//
// Hardware: ESP8266 + Adafruit ST7735 1.8" TFT
// TFT_CS=15, TFT_DC=2, TFT_RST=12, LED=GPIO4, LDR=A0
//
// HELLIGKEITSLOGIK:
//   LDR an A0 ist INVERTIERT:
//     A0 = klein (z.B. <400)  -> es ist HELL  -> LED_PIN = 0   (volle Helligkeit)
//     A0 = gross (z.B. 1023)  -> es ist DUNKEL -> LED_PIN = 253 (minimal, gerade noch sichtbar)
//   analogWrite(LED_PIN, 0)   = Display am hellsten
//   analogWrite(LED_PIN, 254) = Display fast aus
//   analogWrite(LED_PIN, 253) = gerade noch knapp sichtbar
//
//   Mapping: A0 <= 400 -> PWM 0, A0 >= 1023 -> PWM 253, linear dazwischen

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSans9pt7b.h>

#define TFT_PIN_CS   15
#define TFT_PIN_DC   2
#define TFT_PIN_RST  12
#define LED_PIN      4

// Helligkeitssteuerung Grenzen
#define LDR_HELL   400   // A0-Wert bei hellem Licht (oder heller)
#define LDR_DUNKEL 1023  // A0-Wert bei Dunkelheit
#define PWM_HELL   0     // PWM-Wert fuer hellstes Display
#define PWM_DUNKEL 253   // PWM-Wert fuer dunklestes Display (gerade noch sichtbar)

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_PIN_CS, TFT_PIN_DC, TFT_PIN_RST);

int lastValue = -1;

void setup() {
  Serial.begin(9600);
  pinMode(LED_PIN, OUTPUT);
  analogWrite(LED_PIN, PWM_HELL);  // Start mit voller Helligkeit

  tft.initR(INITR_BLACKTAB);
  tft.setTextWrap(false);
  tft.setRotation(1);
  tft.fillScreen(ST7735_BLACK);

  // Header
  tft.setFont();
  tft.setTextColor(ST7735_YELLOW);
  tft.setCursor(2, 2);
  tft.print("LDR Helligkeitstest");
}

void loop() {
  int val = analogRead(A0);

  if (val != lastValue) {
    // PWM berechnen
    int pwm;
    if (val <= LDR_HELL) {
      pwm = PWM_HELL;
    } else if (val >= LDR_DUNKEL) {
      pwm = PWM_DUNKEL;
    } else {
      pwm = map(val, LDR_HELL, LDR_DUNKEL, PWM_HELL, PWM_DUNKEL);
    }
    analogWrite(LED_PIN, pwm);

    // A0-Wert gross anzeigen
    tft.fillRect(0, 14, 160, 46, ST7735_BLACK);
    tft.setFont();
    tft.setTextColor(ST7735_WHITE);
    tft.setCursor(2, 16);
    tft.print("A0:");
    tft.setFont(&FreeSansBold18pt7b);
    tft.setCursor(30, 50);
    char buf[8];
    sprintf(buf, "%4d", val);
    tft.print(buf);

    // PWM-Wert anzeigen
    tft.fillRect(0, 58, 160, 30, ST7735_BLACK);
    tft.setFont();
    tft.setTextColor(ST7735_WHITE);
    tft.setCursor(2, 60);
    tft.print("PWM:");
    tft.setFont(&FreeSans9pt7b);
    tft.setCursor(30, 80);
    sprintf(buf, "%3d", pwm);
    tft.print(buf);

    // Balkenanzeige A0
    tft.fillRect(0, 92, 160, 20, ST7735_BLACK);
    tft.setFont();
    tft.setTextColor(ST7735_YELLOW);
    tft.setCursor(2, 93);
    tft.print("A0");
    int barA0 = map(val, 0, 1024, 0, 138);
    tft.drawRect(20, 93, 138, 10, ST7735_WHITE);
    tft.fillRect(20, 93, barA0, 10, ST7735_CYAN);

    // Balkenanzeige PWM
    tft.fillRect(0, 108, 160, 20, ST7735_BLACK);
    tft.setTextColor(ST7735_YELLOW);
    tft.setCursor(2, 109);
    tft.print("PWM");
    int barPWM = map(pwm, 0, 253, 0, 138);
    tft.drawRect(20, 109, 138, 10, ST7735_WHITE);
    tft.fillRect(20, 109, barPWM, 10, ST7735_GREEN);

    lastValue = val;
    Serial.print("A0=");
    Serial.print(val);
    Serial.print(" PWM=");
    Serial.println(pwm);
  }

  delay(200);
}
