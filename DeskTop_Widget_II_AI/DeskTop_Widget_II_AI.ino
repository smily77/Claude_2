#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiUdp.h>
#include <Timezone.h>
#include <ArduinoJson.h>
#include <ESP8266TrueRandom.h>
#include <LovyanGFX.hpp>

// --- Display-Konfiguration für ESP8266 + ST7735 ---
class LGFX_ESP8266_ST7735 : public lgfx::LGFX_Device {
    lgfx::Panel_ST7735S _panel_instance;
    lgfx::Bus_SPI _bus_instance;
    
public:
    LGFX_ESP8266_ST7735(void) {
        // SPI Bus-Konfiguration für ESP8266
        {
            auto cfg = _bus_instance.config();
            // ESP8266 spezifische Konfiguration
            cfg.spi_mode = 0;
            cfg.freq_write = 27000000;    // 27 MHz Schreibgeschwindigkeit
            cfg.freq_read  = 16000000;
            cfg.spi_3wire = false;
            cfg.pin_sclk = 14;            // GPIO 14 = D5 (SCK)
            cfg.pin_mosi = 13;            // GPIO 13 = D7 (MOSI)
            cfg.pin_miso = -1;            // Nicht verwendet
            cfg.pin_dc   = 2;             // GPIO 02 = D4 (DC)
            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }
        
        // Panel-Konfiguration ST7735
        {
            auto cfg = _panel_instance.config();
            cfg.pin_cs   = 15;            // GPIO 15 = D8 (CS)
            cfg.pin_rst  = 12;            // GPIO 12 = D6 (RST)
            cfg.pin_busy = -1;            // Nicht verwendet
            cfg.memory_width  = 128;
            cfg.memory_height = 160;
            cfg.panel_width  = 128;
            cfg.panel_height = 160;
            cfg.offset_x = 0;
            cfg.offset_y = 0;
            cfg.offset_rotation = 0;
            cfg.dummy_read_pixel = 8;
            cfg.dummy_read_bits  = 1;
            cfg.readable = false;
            cfg.invert = false;
            cfg.rgb_order = false;
            cfg.dlen_16bit = false;
            cfg.bus_shared = false;
            _panel_instance.config(cfg);
        }
        
        setPanel(&_panel_instance);
    }
};

// Display-Instanz
LGFX_ESP8266_ST7735 tft;

#define BL 0 // Backlight
// Sprites für flackerfreie Updates
lgfx::LGFX_Sprite timeSprite(&tft);     // Für Zeit/Datum
lgfx::LGFX_Sprite citySprite(&tft);     // Für Städte-Zeiten  
lgfx::LGFX_Sprite rateSprite(&tft);     // Für Wechselkurse

// --- 1. WICHTIG: KONFIGURATION ANPASSEN! ---
#include <Credentials.h> // Definiert ssid, password und fixerApiKey
//const char* ssid
//const char* password
//const char* fixerApiKey 

const char* FX_API_HOST = "api.exchangerate.host";
const char* FX_API_ENDPOINT = "/latest?base=CHF&symbols=USD,EUR,GBP"; 

// --- Tägliches Update-Schema ---
const long UPDATE_INTERVAL_SECONDS = 24 * 3600; 
const int JITTER_SECONDS = 30 * 60;

// --- Statische Variable ---
float usd_chf = 0.80; 
float eur_chf = 0.94;
float gbp_chf = 1.07;
long last_successful_update = 0; 

// NTP-Client-Setup
const char* ntpServer = "pool.ntp.org";
time_t current_time = 0;

// --- DST/Zeitzonen-Definitionen ---
TimeChangeRule cest = {"CEST", Last, Sun, Mar, 2, 120}; 
TimeChangeRule cet  = {"CET",  Last, Sun, Oct, 3, 60};  
Timezone zurich_tz(cest, cet);

TimeChangeRule rules[] = {
    {"DBX", Last, Sun, Oct, 3, 240}, {"DBX", Last, Sun, Oct, 3, 240}, 
    {"SIN", Last, Sun, Oct, 3, 480}, {"SIN", Last, Sun, Oct, 3, 480}, 
    {"EDT", Second, Sun, Mar, 2, -240}, {"EST", First, Sun, Nov, 2, -300},  
    {"AEDT", First, Sun, Oct, 2, 660}, {"AEST", First, Sun, Apr, 3, 600},  
    {"BLR", Last, Sun, Oct, 3, 330}, {"BLR", Last, Sun, Oct, 3, 330}, 
    {"PDT", Second, Sun, Mar, 2, -420}, {"PST", First, Sun, Nov, 2, -480}
};

struct CityConfig {
    const char* code;
    Timezone tz;
};

CityConfig cities[] = {
    {"DBX", Timezone(rules[0], rules[1])},
    {"SIN", Timezone(rules[2], rules[3])},
    {"IAD", Timezone(rules[4], rules[5])},
    {"SYD", Timezone(rules[6], rules[7])},
    {"BLR", Timezone(rules[8], rules[9])},
    {"SFO", Timezone(rules[10], rules[11])}
};
const int numCities = sizeof(cities) / sizeof(cities[0]);

// --- HILFSFUNKTIONEN ---

// Stellt die WLAN-Verbindung her
bool _connectWifi() {
    if (WiFi.status() == WL_CONNECTED) return true;

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    Serial.print("Verbinde mit WLAN");
    
    int timeout = 0;
    while (WiFi.status() != WL_CONNECTED && timeout < 20) {
        delay(500);
        Serial.print(".");
        timeout++;
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("\nFEHLER: Keine WLAN-Verbindung möglich.");
        return false;
    }
    Serial.println("\nWLAN verbunden.");
    return true;
}

// Trennt die WLAN-Verbindung
void _disconnectWifi() {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    Serial.println("WLAN getrennt.");
}

// --- 2. FUNKTIONEN ---

// Ruft NTP-Zeit ab (WLAN muss bereits verbunden sein!)
bool syncTimeNTP() {
    Serial.println("Starte NTP-Synchronisation...");
    
    configTime(0, 0, ntpServer); 
    
    for (int i = 0; i < 20; i++) {
        time_t t = time(nullptr);
        if (t > 1672531200) { // Zeit nach 1.1.2023
            current_time = t;
            Serial.printf("NTP erfolgreich: %s", ctime(&current_time));
            return true;
        }
        delay(500);
    }
    
    Serial.println("Fehler: NTP-Synchronisation fehlgeschlagen.");
    return false;
}

// Ruft Wechselkurse von der API ab (WLAN muss bereits verbunden sein!)
bool getExchangeRates() {
    Serial.println("Rufe Wechselkurse ab...");
    
    WiFiClient client;
    HTTPClient http;
    
    String url = "http://" + String(FX_API_HOST) + String(FX_API_ENDPOINT); 
    
    http.begin(client, url);
    int httpCode = http.GET(); 

    bool success = false;
    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        
        DynamicJsonDocument doc(512);
        DeserializationError error = deserializeJson(doc, payload);

        if (!error && doc.containsKey("rates")) {
            usd_chf = doc["rates"]["USD"].as<float>();
            eur_chf = doc["rates"]["EUR"].as<float>();
            gbp_chf = doc["rates"]["GBP"].as<float>();
            
            Serial.printf("Neue Kurse: USD %.4f, EUR %.4f, GBP %.4f\n", usd_chf, eur_chf, gbp_chf);
            success = true;
        } else {
            Serial.println("API-Antwort ungültig oder Fehler gemeldet.");
        }
    } else {
        Serial.printf("HTTP-Fehlercode: %d\n", httpCode);
    }

    http.end();
    return success;
}

// Führt den vollständigen Update-Zyklus aus
bool performFullUpdateCycle() {
    Serial.println("=== Starte vollständigen Update-Zyklus ===");
    
    if (!_connectWifi()) {
        Serial.println("Update-Zyklus abgebrochen: Keine WLAN-Verbindung.");
        return false;
    }
    
    bool timeSuccess = syncTimeNTP();
    bool fxSuccess = false;

    if (timeSuccess) {
        fxSuccess = getExchangeRates();
    } else {
        Serial.println("Wechselkurs-Update übersprungen wegen fehlgeschlagener Zeit-Synchronisation.");
    }
    
    _disconnectWifi();

    if (timeSuccess && fxSuccess) {
        last_successful_update = current_time;
        Serial.println("=== Update-Zyklus ERFOLGREICH abgeschlossen ===");
        return true;
    } else {
        Serial.println("=== Update-Zyklus FEHLGESCHLAGEN ===");
        return false;
    }
}

// Prüft, ob ein Update nötig ist
void checkAndUpdate() {
    time_t now = time(nullptr);
    
    long jitter = ESP8266TrueRandom.random(JITTER_SECONDS);
    long required_update_time = last_successful_update + UPDATE_INTERVAL_SECONDS + jitter;

    if (last_successful_update == 0) {
        Serial.println("*** NEUSTART ERKANNT - Initiales Update erforderlich ***");
        performFullUpdateCycle();
    } 
    else if (now < 1672531200) {
        Serial.println("*** ZEIT UNGÜLTIG - Update erforderlich ***");
        performFullUpdateCycle();
    }
    else if (now > required_update_time) {
        Serial.println("*** TÄGLICHES UPDATE FÄLLIG ***");
        performFullUpdateCycle();
    } 
    else {
        current_time = now;
        long seconds_to_update = required_update_time - now;
        Serial.printf("Nächstes Update in %ld:%02ld:%02ld\n", 
                      seconds_to_update/3600, 
                      (seconds_to_update%3600)/60, 
                      seconds_to_update%60);
    }
    
    updateExchangeRatesDisplay();
}

// --- Anzeige-Funktionen mit LovyanGFX ---

void drawScreenLayout() {
    // Bildschirm löschen
    tft.fillScreen(TFT_BLACK);
    
    // Trennlinien
    tft.drawFastHLine(0, 30, tft.width(), TFT_BLUE);
    tft.drawFastHLine(0, 100, tft.width(), TFT_BLUE);
    
    // Header für Währungen (direkt auf Display)
    tft.setFont(&fonts::Font2);  // Kleine Schrift
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    
    tft.setCursor(5, 110);  tft.print("USD");
    tft.setCursor(50, 110); tft.print("EUR");
    tft.setCursor(95, 110); tft.print("GBP");
    
    // Header für Zeitzonen (direkt auf Display)
    tft.setCursor(10, 40); tft.print("DBX");
    tft.setCursor(60, 40); tft.print("SIN");
    tft.setCursor(110, 40); tft.print("IAD");
    
    tft.setCursor(10, 70); tft.print("SYD");
    tft.setCursor(60, 70); tft.print("BLR");
    tft.setCursor(110, 70); tft.print("SFO");
}

void updateTimeDisplay() {
    // Sprite für flackerfreie Zeit-Anzeige
    timeSprite.fillSprite(TFT_BLACK);
    
    if (current_time < 1672531200) {
        // Ungültige Zeit
        timeSprite.setFont(&fonts::Font6);  // Große Schrift
        timeSprite.setTextColor(TFT_WHITE);
        timeSprite.setCursor(5, 5);
        timeSprite.print("--:--");
    } else {
        // Zürich Zeit
        time_t local_time = zurich_tz.toLocal(current_time);
        struct tm * ti = localtime(&local_time);
        
        // Große Zeit
        timeSprite.setFont(&fonts::Font6);  // Große Schrift für Zeit
        timeSprite.setTextColor(TFT_WHITE);
        timeSprite.setCursor(5, 5);
        timeSprite.printf("%02d:%02d", ti->tm_hour, ti->tm_min);
        
        // Datum
        char date_str[10];
        strftime(date_str, sizeof(date_str), "%d %b", ti);
        timeSprite.setFont(&fonts::Font4);  // Mittlere Schrift für Datum
        timeSprite.setCursor(100, 10);
        timeSprite.print(date_str);
    }
    
    // Sprite auf Display pushen (flackerfrei!)
    timeSprite.pushSprite(0, 0);
    
    // Weltstädte-Zeiten mit zweitem Sprite
    if (current_time >= 1672531200) {
        citySprite.fillSprite(TFT_BLACK);
        citySprite.setFont(&fonts::Font4);  // Mittlere Schrift
        citySprite.setTextColor(TFT_YELLOW);
        
        for (int i = 0; i < numCities; i++) {
            time_t city_local_time = cities[i].tz.toLocal(current_time);
            struct tm * city_ti = localtime(&city_local_time);
            
            int row = (i < 3) ? 20 : 50;  // Relativ zum Sprite
            int col = (i % 3) * 50 + 10;
            
            citySprite.setCursor(col, row);
            citySprite.printf("%02d:%02d", city_ti->tm_hour, city_ti->tm_min);
        }
        
        citySprite.pushSprite(0, 50);  // Position unterhalb der Zeit
    }
}

void updateExchangeRatesDisplay() {
    // Sprite für flackerfreie Kurs-Anzeige
    rateSprite.fillSprite(TFT_BLACK);
    
    rateSprite.setFont(&fonts::Font4);  // Mittlere Schrift
    rateSprite.setTextColor(TFT_GREEN);
    
    // USD
    rateSprite.setCursor(5, 5);
    rateSprite.printf("%.2f", usd_chf);
    
    // EUR
    rateSprite.setCursor(50, 5);
    rateSprite.printf("%.2f", eur_chf);
    
    // GBP
    rateSprite.setCursor(95, 5);
    rateSprite.printf("%.2f", gbp_chf);
    
    // Sprite auf Display pushen
    rateSprite.pushSprite(0, 125);
}

// --- 3. HAUPTPROGRAMM ---

void setup() {
    Serial.begin(115200);
    Serial.println("\n\n========================================");
    Serial.println("   Worldclock mit Wechselkursen v2.0");
    Serial.println("        (LovyanGFX Edition)");
    Serial.println("========================================");
    Serial.println("Pin-Konfiguration:");
    Serial.println("  CS   = GPIO 15 (D8)");
    Serial.println("  DC   = GPIO 02 (D4)");  
    Serial.println("  RST  = GPIO 12 (D6)");
    Serial.println("  SCK  = GPIO 14 (D5)");
    Serial.println("  MOSI = GPIO 13 (D7)");
    Serial.println("  BL   = GPIO 00 (D3)"); 
    Serial.println("========================================");
    
    // Display initialisieren
    tft.init();
    tft.setRotation(3);  // Landscape
    pinMode(BL,OUTPUT);
    digitalWrite(BL, LOW);
    tft.fillScreen(TFT_BLACK);
    
    // Sprites erstellen (einmalige Allokation!)
    timeSprite.createSprite(160, 30);   // Oberer Bereich
    citySprite.createSprite(160, 70);   // Mittlerer Bereich  
    rateSprite.createSprite(160, 30);   // Unterer Bereich
    
    // Statisches Layout zeichnen
    drawScreenLayout();
    
    Serial.println("Display initialisiert.");
    Serial.println("Starte initiale Aktualisierung...");
    
    // checkAndUpdate erkennt last_successful_update == 0 
    // und führt automatisch den ersten Update-Zyklus durch
    checkAndUpdate();
    
    // Erste Anzeige
    updateTimeDisplay();
    updateExchangeRatesDisplay();
    
    Serial.println("Setup abgeschlossen. Hauptschleife startet...");
}

void loop() {
    time_t now = time(nullptr);

    // Zeit-Update nur wenn sich etwas geändert hat
    if (now != current_time) {
        current_time = now;
        
        // Display aktualisieren
        updateTimeDisplay();
        
        // Einmal pro Minute prüfen ob Update nötig ist
        struct tm * ti = localtime(&now);
        if (ti->tm_sec == 0) {
            checkAndUpdate(); 
        }
    }
    
    delay(50);
}
