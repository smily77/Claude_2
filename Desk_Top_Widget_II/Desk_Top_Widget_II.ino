// 9l Washington corrected (mit ESP8266 Package 2.7.4)
// 9k neuer API für Exchangerate
// 9i neuer fixer API 
// 9h -Geschäft neu mit Mobile
// 9f MAC adresse defined & R&M Guest Access
// 9e . Bei IAD eingefügt
// 9d Mit IAD statt SAL
// 9c published Version
// 9b mit timezonedb.com
// 9 Mit http://api.apixu.com statt worldweatheronline
// 8x: Wie 8 aber zum Fehler sucen
// 8: neuer Watchdog versuch
// 7: Ohne Watchdog; visability für Hänger
// 6: Watchdog
// 5c: Für alle modelle
// 5b: Suche nach Fehler bei New
// 5: mit screentest
// 4: Formularauswertung
// 3: On base of Trial_NTP_10
#include <TimeLib.h> 
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiUdp.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h> 
#include <SFE_BMP180.h>
#include <Wire.h>
#include <TextFinder.h>
#include <Streaming.h>
#include <Ticker.h>
#include <Fonts/FreeSans9pt7b.h>
//#include <Fonts/FreeSans12pt7b.h>
//#include <Fonts/FreeSans18pt7b.h>
//#include <Fonts/FreeSans24pt7b.h>
//#include <Fonts/FreeSansBold9pt7b.h>
//#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
//#include <Fonts/FreeSansBold24pt7b.h>
//#include <Fonts/FreeSansOblique9pt7b.h>
//#include <Fonts/FreeSansOblique12pt7b.h>
//#include <Fonts/FreeSansOblique18pt7b.h>
//#include <Fonts/FreeSansOblique24pt7b.h>
//#include <Fonts/FreeSansBoldOblique9pt7b.h>
//#include <Fonts/FreeSansBoldOblique12pt7b.h>
//#include <Fonts/FreeSansBoldOblique18pt7b.h>
//#include <Fonts/FreeSansBoldOblique24pt7b.h>
extern "C" {
  #include "user_interface.h"
}
//#define OLD
#define NEW
#define SCHWARZ
#define DEBUG true
#define screenTest false

SFE_BMP180 pressure;
char status;
double T,P;

WiFiClient client;
TextFinder finder(client);

WiFiClientSecure clientSec;
#include <Credentials.h>
#define maxWlanTrys 100

//const char* timerServerDNSName = "0.europe.pool.ntp.org";
const char* timerServerDNSName = "0.ch.pool.ntp.org";
IPAddress timeServer;

WiFiUDP Udp;
const unsigned int localPort = 8888;  // local port to listen for UDP packets
const int NTP_PACKET_SIZE = 48;       // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE];   // buffer to hold incoming & outgoing packets

#define TFT_PIN_CS   15
#define TFT_PIN_DC   2
#define TFT_PIN_RST  12

#if defined(SCHWARZ)
  #define ledPin 0
#else
  #define ledPin 4
#endif  

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_PIN_CS, TFT_PIN_DC, TFT_PIN_RST);

WiFiServer server(80);
unsigned long ulReqcount;
unsigned long ulReconncount;


time_t currentTime =0;
char anzeige[24];
int secondLast, minuteLast;
byte icon, humidity, temp;
char iconSub;
boolean gotWeather, gotForecast;
String  strBuf, strBuf2;
float lat, lon, utc;
String location[7];
String airCode[7];
long utcOffset[7];
String fxSym[4];
float fxValue[4];
int helligkeit;
boolean firstRun = true;
int wPeriode;
char buf[40];
int h1,h2;
boolean webInp = false;

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
  tft.setTextColor( ST7735_RED );
  tft.setTextSize(2);
  tft.println("WATCHDOG");
  tft.println("ATTACK");
  delay(10000);
  ESP.reset();
}

void setup() {
  if (DEBUG) Serial.begin(9600);
  location[0] = "Herisau,ch"; //Origin
  location[1] = "Dubai,ae";
  airCode[1] = "DBX";
  location[2] = "Singapore,sg";
  airCode[2] = "SIN";
  location[3] = "Arlington,us";
 // location[3] = "Washington DC., us";
  airCode[3] = "IAD";
  location[4] = "Sydney,au";
  airCode[4] = "SYD";
  location[5] = "Bangalore,in";
  airCode[5] = "BLR";
  location[6] = "Sausalito,us";
  airCode[6] = "SFO";
  
  fxSym[0] = "CHF";  //Origin
  fxSym[1] = "USD";
  fxSym[2] = "EUR";
  fxSym[3] = "GBP";
  
#if defined(NEW)
  pinMode(ledPin,OUTPUT);
  digitalWrite(ledPin,LOW);
#endif

  tft.initR(INITR_BLACKTAB);
  tft.setTextWrap( false );
  tft.setTextColor( ST7735_WHITE );
  tft.setRotation( 1 );
#if defined(NEW)
  tft.setRotation( 1 );
#elif defined(OLD)
  tft.setRotation( 3 );
#endif
  tft.fillScreen( ST7735_BLACK );
  tft.setTextColor(ST7735_WHITE,ST7735_BLACK);
  tft.setTextSize(1);
  tft.setCursor( 0, 0 );

  tft.print( "Searching for: ");
  if (DEBUG) tft.print(ssid1);
    else tft.print(ssid2);
  tft.setCursor( 0, 16 );

//uint8_t mac[6] {0x18, 0xfe, 0x34, 0xa2, 0x21, 0xd2}; //orginal
//uint8_t mac[6] {0x18, 0xe2, 0xc2, 0xea, 0x90, 0x75}; //Android Handy
//wifi_set_macaddr(STATION_IF, mac);


if (!screenTest) {      
    if (DEBUG) WiFi.begin(ssid1, password1); 
      else WiFi.begin(ssid2, password2);    
    wifi_station_set_auto_connect(true);
wlanInitial:  
    wPeriode = 1;
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      tft.print(".");
      wPeriode++;
      if (wPeriode > maxWlanTrys) {
        tft.println();
        tft.println("Break due to missing WLAN");
        if (String(ssid1) == WiFi.SSID()) {
          tft.print("Switch to: ");
          tft.println(ssid2);
          WiFi.begin(ssid2, password2);
          wifi_station_set_auto_connect(true);
        }
        else {
          tft.print("Switch to: ");
          tft.println(ssid1);
          WiFi.begin(ssid1, password1);
          wifi_station_set_auto_connect(true);
        }
        goto wlanInitial;
      }  
    }
  }
  tft.setCursor( 0, 32 );
  tft.println("WiFi connected");  
  tft.println("IP address: ");
  
  clearTFTScreen();

  tft.println(WiFi.localIP());
  tft.println();
  tft.print("MAC: ");
  tft.println(WiFi.macAddress());
  if (DEBUG) {
    Serial.print("MAC; ");
    Serial.println(WiFi.macAddress());

  }
  if (!screenTest) catchTimes();
 
  clearTFTScreen();
 
  if (!screenTest) {
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
  }
  clearTFTScreen();
  if (!screenTest) catchCurrencies();
  
  firstRun = false;
  secondLast = second();
  minuteLast = minute();
  if (!screenTest) displayMainScreen();
    else {
      tft.drawRect(0,0,160,128,ST7735_WHITE);
      tft.drawRect(1,1,158,126,ST7735_BLUE);
      tft.drawRect(2,2,156,124,ST7735_RED);
      tft.drawRect(3,3,154,122,ST7735_YELLOW);
      tft.drawRect(4,4,152,120,ST7735_GREEN);
      tft.drawRect(5,5,150,118,ST7735_CYAN);
      tft.drawRect(6,6,148,116,ST7735_BLACK);
      tft.drawRect(7,7,146,114,ST7735_WHITE);
      tft.drawRect(8,8,144,112,ST7735_YELLOW);
      tft.drawRect(9,9,142,110,ST7735_RED);
  }

#if defined(SCHWARZ)
  Wire.begin(4, 5);
  pressure.begin();
#else 
   if (!DEBUG) {
     Wire.begin(1, 3);
     pressure.begin();
   }
#endif
  server.begin();
  secondTick.attach(1,ISRwatchDog);
} //Setup

void loop(){
  if (!screenTest) {
    if (minuteLast != minute()) {
      minuteLast = minute();
      if (DEBUG) Serial.print(hour());
      if (DEBUG) Serial.print(":");
      if (DEBUG) Serial.println(minute());
      if ((hour()== (hour()/4) *4 ) && (minuteLast == 0)) {
//      if (minuteLast == 00) {
        if (firstRun) clearTFTScreen();
        catchTimes();
        if (firstRun) clearTFTScreen();
        if (DEBUG) Serial.println("Times catched");
        do {
          currentTime = getNtpTime();
        } while (currentTime == 0);
        if (DEBUG) Serial.println("NTP catched");
        if (firstRun) clearTFTScreen();
        catchCurrencies();
        if (DEBUG) Serial.println("Currencies catched");
      }
      displayMainScreen(); 
    } 
 
    client = server.available();
    if (client) {
      boolean currentLineIsBlank = true;
      if (client.connected()) {
        while(!client.available());
        if (finder.findUntil("city","HTTP")) {
          h1 = finder.getString("=","&",buf,40);
          if (DEBUG) Serial.print(buf);
          if (DEBUG) Serial.print("  ");
          if (DEBUG) Serial.println(h1);
          location[6] = "";
          for (h2 = 0; h2 < h1; h2++) {
            if (buf[h2] != '%') location[6] += buf[h2];
              else {
                location[6] += ",";
                h2++;
                h2++;
              } // else
          } // for
          if (finder.findUntil("airPort","HTTP")) {
            webInp = true;
            h1 = finder.getString("=","&",buf,40);
            if (DEBUG) Serial.print(buf);
            if (DEBUG) Serial.print("  ");
            if (DEBUG) Serial.println(h1);
            airCode[6]="";
            for (h2 = 0; h2 < h1; h2++) {
              airCode[6] += buf[h2];
            } // for
            finder.findUntil("curr","HTTP"); 
            h1 = finder.getString("="," ",buf,40);
            if (DEBUG) Serial.print(buf);
            if (DEBUG) Serial.print("  ");
            if (DEBUG) Serial.println(h1);
            fxSym[3]="";
            for (h2 = 0; h2 < h1; h2++) {
              fxSym[3] += buf[h2];
            } // for
          } // airPort
        } //city
       } 
      while (client.connected()) {  
       if (client.available()) {        
          char c = client.read();
          if (DEBUG) Serial.print(c);
          if (c == '\n' && currentLineIsBlank) break;
          if (c == '\n') currentLineIsBlank = true; else if (c != '\r') currentLineIsBlank = false;
        } // if available
      } //while
      printWeb();
      if (webInp) {
        if (DEBUG) {
          Serial.println(location[6]);
          Serial.println(airCode[6]);
          Serial.println(fxSym[3]);
        }
        catchTimes();
        catchCurrencies();
        webInp = false;
        displayMainScreen(); 
      }
    } // webinterface
  } // (!screenTest}
  else {
    tft.setTextSize(2);
    tft.setCursor(50,50);   
#if defined(NEW)
    helligkeit = analogRead(A0);
#else
    helligkeit = 999;
#endif
    if (helligkeit < 1000) tft.print(" ");  
    if (helligkeit < 100) tft.print(" ");  
    if (helligkeit < 10) tft.print(" ");  
    tft.println(helligkeit);
  }
     
#if defined(NEW)  
  helligkeit = analogRead(A0);
  delay(100);
  if (helligkeit > 1010) helligkeit = 1010;
  analogWrite(ledPin,helligkeit);
#endif
  watchDogFeed();
}
