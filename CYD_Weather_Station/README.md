# CYD Wetterstation mit 24-Stunden-Graphen

Eine schöne Wetterstation für das CYD (Cheap Yellow Display) mit BMP280 und AHT20 Sensoren.

## Features

- ✅ Große, gut lesbare Schrift für aktuelle Werte
- ✅ Echtzeit-Anzeige von Temperatur, Luftfeuchtigkeit und Luftdruck
- ✅ 24-Stunden-Verlaufsdiagramme für alle drei Werte
- ✅ Automatische Datenerfassung alle 15 Minuten (96 Datenpunkte)
- ✅ Display-Update alle 10 Sekunden
- ✅ Schöne Farb-Kodierung für die verschiedenen Messwerte

## Voraussetzungen

### CYD_Display_Config.h
**WICHTIG:** Du musst deine eigene `CYD_Display_Config.h` bereitstellen, die:
- Die `LGFX` Klasse für dein spezifisches CYD-Board definiert
- LovyanGFX korrekt konfiguriert
- Optional `extSDA` und `extSCL` für I2C Pins definiert (Standard: GPIO 22 und 27)

### Sensoren (I2C)
- **BMP280** (Adresse 0x77) - Temperatur & Luftdruck
- **AHT20** (Adresse 0x38) - Temperatur & Luftfeuchtigkeit

### Verkabelung

```
CYD         -> Sensoren
GPIO 22 (SDA) -> SDA (BMP280 & AHT20)
GPIO 27 (SCL) -> SCL (BMP280 & AHT20)
3.3V          -> VCC
GND           -> GND
```

## Benötigte Bibliotheken

Installation über Arduino Library Manager:

1. **LovyanGFX** by lovyan03
2. **Adafruit BMP280 Library** by Adafruit
3. **Adafruit AHTX0** by Adafruit

### Abhängigkeiten (werden automatisch installiert)
- Adafruit Unified Sensor
- Adafruit BusIO

## Installation

1. Stelle sicher, dass deine `CYD_Display_Config.h` vorhanden ist
2. Bibliotheken über Arduino Library Manager installieren:
   - LovyanGFX
   - Adafruit BMP280 Library
   - Adafruit AHTX0
3. Sketch in Arduino IDE öffnen
4. Board-Einstellungen für dein ESP32-Board wählen
5. Upload!

## Anzeige

### Oberer Bereich
Drei große Boxen zeigen die aktuellen Werte:
- **Temperatur** (Orange) - in °C (Durchschnitt aus BMP280 und AHT20)
- **Luftfeuchtigkeit** (Cyan) - in %
- **Luftdruck** (Grün) - in hPa

### Unterer Bereich
Drei kompakte Graphen zeigen den 24-Stunden-Verlauf:
- **T-Graph** (Orange) - Temperatur (-10°C bis 40°C)
- **H-Graph** (Cyan) - Luftfeuchtigkeit (0% bis 100%)
- **P-Graph** (Grün) - Luftdruck (950 hPa bis 1050 hPa)

## Datenerfassung

- **Sensor-Update**: Alle 10 Sekunden
- **Display-Update**: Alle 10 Sekunden
- **Daten-Speicherung**: Alle 15 Minuten
- **Datenpunkte**: 96 (= 24 Stunden bei 15-Minuten-Intervall)
- **Ring-Puffer**: Älteste Werte werden automatisch überschrieben

## Anpassungen

### I2C Pins ändern
Option 1: In deiner `CYD_Display_Config.h`:
```cpp
#define extSDA 22  // Ändere hier die SDA Pin-Nummer
#define extSCL 27  // Ändere hier die SCL Pin-Nummer
```

Option 2: Direkt im Sketch - ändere die Fallback-Werte:
```cpp
#ifndef extSDA
#define extSDA 22  // Deine SDA Pin-Nummer
#endif
#ifndef extSCL
#define extSCL 27  // Deine SCL Pin-Nummer
#endif
```

### Messintervalle ändern
In `CYD_Weather_Station.ino`:
```cpp
const unsigned long SENSOR_INTERVAL = 10000;   // 10 Sekunden
const unsigned long STORE_INTERVAL = 900000;   // 15 Minuten
```

### Anzahl Datenpunkte ändern
In `CYD_Weather_Station.ino`:
```cpp
#define DATA_POINTS 96  // 24h bei 15min = 96 Punkte
```

### Farben anpassen
In `CYD_Weather_Station.ino`:
```cpp
#define COLOR_TEMP     0xFD20  // Orange für Temperatur
#define COLOR_HUM      0x07FF  // Cyan für Feuchtigkeit
#define COLOR_PRESS    0x07E0  // Grün für Luftdruck
```

## Troubleshooting

### Sensoren werden nicht erkannt
1. I2C Verkabelung prüfen
2. I2C Scanner ausführen (Beispiel in Arduino IDE)
3. Richtige I2C Adressen prüfen (0x77 für BMP280, 0x38 für AHT20)
4. Pull-Up Widerstände prüfen (oft schon auf Sensor-Boards vorhanden)

### Display bleibt schwarz
1. CYD_Display_Config.h prüfen (LGFX Klasse korrekt konfiguriert?)
2. Backlight-Konfiguration in CYD_Display_Config.h prüfen
3. SPI Verbindung prüfen

## Serielle Ausgabe

Bei 115200 Baud werden Debug-Informationen ausgegeben:
- Sensor-Initialisierung
- Aktuelle Messwerte
- Gespeicherte Datenpunkte

## Lizenz

Frei verwendbar für private und kommerzielle Projekte.

## Credits

- LovyanGFX Library: https://github.com/lovyan03/LovyanGFX
- Adafruit Sensor Libraries: https://github.com/adafruit
