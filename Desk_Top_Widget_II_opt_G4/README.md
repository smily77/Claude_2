# Desk_Top_Widget_II_opt_G4 - Version fuer 4G-Hotspot

Weiterentwicklung von Desk_Top_Widget_II_opt, angepasst fuer den Betrieb
mit einem mobilen 4G-Hotspot der sein WLAN bei Inaktivitaet abschaltet.

## Aenderungen gegenueber Desk_Top_Widget_II_opt

### 1. WiFi Keep-Alive (NEU)
Der 4G-Hotspot schaltet sein WLAN ab wenn keine Geraete verbunden sind.
Loesung: Alle 3 Minuten verbindet sich der ESP8266 kurz (2 Sek.) mit dem
WLAN und trennt wieder. Dies geschieht **nicht-blockierend im Hintergrund**
und stoert die Uhrzeitanzeige nicht.

### 2. Vereinfachte WLAN-Auswahl
- `DEBUG true`: Verwendet ssid1/password1
- `DEBUG false`: Verwendet ssid2/password2
- Kein automatisches Umschalten mehr bei Verbindungsproblemen
- Bei Verbindungsfehler beim Start: Neustart nach 10 Sekunden

### 3. WiFi-Status-Anzeige
- Staedtenamen und Zeiten werden **ROT** angezeigt wenn der letzte
  WiFi Keep-Alive fehlgeschlagen ist
- Zurueck zu normalen Farben (GELB/WEISS) bei erfolgreicher Verbindung

### 4. Wechselkurs-Anzeige
- Wechselkurse werden **ausgeblendet** wenn das Laden fehlgeschlagen ist
- Sobald ein Update erfolgreich ist, werden sie wieder angezeigt

### 5. Taegliche Aktualisierung um 17:00
- Statt um Mitternacht wird jetzt um 17:00 Uhr aktualisiert
- Grund: Die EZB aktualisiert Wechselkurse nach 16:00 Uhr

### 6. Korrekte Helligkeitssteuerung
Die LDR-Logik ist invertiert (im Original falsch implementiert):
- A0 klein (<=400) = hell -> Display hell (PWM=0)
- A0 gross (>=1023) = dunkel -> Display dunkel (PWM=253)

### 7. WiFi-Management
- Nach dem initialen Setup wird WiFi getrennt
- Verbindung nur fuer Keep-Alive (3 Min.) und taegliches Update (17:00)
- Minimaler Datenverbrauch (Keep-Alive uebertraegt keine Daten)

## Konfiguration

### WiFi
In `Credentials.h` (im Arduino Libraries Verzeichnis):
```cpp
const char* ssid1 = "HeimWiFi";          // Fuer DEBUG
const char* password1 = "passwort1";
const char* ssid2 = "4G-Hotspot";        // Fuer Produktion
const char* password2 = "passwort2";
```

### Keep-Alive Intervall
In `Desk_Top_Widget_II_opt_G4.ino`:
```cpp
const unsigned long KEEPALIVE_INTERVAL_MS = 3UL * 60UL * 1000UL;  // 3 Minuten
```

### Staedte / Airport-Codes
```cpp
const char* AIRPORT_CODES[7] = {
  "ZRH",  // 0: Lokale Zeit
  "DXB",  // 1-6: Weltstaedte
  "SIN", "IAD", "SYD", "BLR", "SFO"
};
```

## Hardware

Identisch zu Desk_Top_Widget_II_opt:
- ESP8266 (NodeMCU / WeMos D1 Mini)
- Adafruit ST7735 1.8" TFT (160x128)

### Pin-Belegung
```
TFT_CS  = GPIO 15
TFT_DC  = GPIO 2
TFT_RST = GPIO 12
LED     = GPIO 4  (Hintergrundbeleuchtung)
LDR     = A0      (Helligkeitssensor)
```

## Dateien
| Datei | Inhalt |
|-------|--------|
| Desk_Top_Widget_II_opt_G4.ino | Hauptprogramm, Setup, Loop, Keep-Alive |
| x_printsubs.ino | Display-Funktionen |
| X_InternetInfo.ino | Wechselkurs-Abruf |
| x_subroutines.ino | Timezone, NTP, DST |
| AirportDatabase.h | Airport-Datenbank (PROGMEM) |
