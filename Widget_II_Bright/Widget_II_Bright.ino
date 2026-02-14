// Widget_II_Bright
// Zeigt den Analogwert des LDR (A0) auf dem TFT Display
// Zur Kalibrierung der Helligkeitssteuerung
//
// Hardware: ESP8266 + Adafruit ST7735 1.8" TFT
// TFT_CS=15, TFT_DC=2, TFT_RST=12, LED=GPIO4, LDR=A0

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <Fonts/FreeSansBold18pt7b.h>

#define TFT_PIN_CS   15
#define TFT_PIN_DC   2
#define TFT_PIN_RST  12
#define LED_PIN      4

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_PIN_CS, TFT_PIN_DC, TFT_PIN_RST);

int lastValue = -1;

void setup() {
  Serial.begin(9600);
  pinMode(LED_PIN, OUTPUT);
  analogWrite(LED_PIN, 1010);  // Display-Helligkeit fix auf Maximum

  tft.initR(INITR_BLACKTAB);
  tft.setTextWrap(false);
  tft.setRotation(1);
  tft.fillScreen(ST7735_BLACK);

  tft.setFont();
  tft.setTextColor(ST7735_YELLOW);
  tft.setCursor(2, 2);
  tft.print("LDR Helligkeitstest");
  tft.setCursor(2, 16);
  tft.setTextColor(ST7735_WHITE);
  tft.print("Analogwert A0 (0-1024):");
}

void loop() {
  int val = analogRead(A0);

  if (val != lastValue) {
    // Zahlenbereich loeschen und neu zeichnen
    tft.fillRect(0, 40, 160, 50, ST7735_BLACK);
    tft.setFont(&FreeSansBold18pt7b);
    tft.setTextColor(ST7735_WHITE);
    tft.setCursor(20, 78);
    char buf[8];
    sprintf(buf, "%4d", val);
    tft.print(buf);

    // Balkenanzeige
    tft.fillRect(0, 95, 160, 16, ST7735_BLACK);
    int barWidth = map(val, 0, 1024, 0, 156);
    tft.fillRect(2, 97, barWidth, 12, ST7735_GREEN);

    lastValue = val;
    Serial.println(val);
  }

  delay(200);
}
