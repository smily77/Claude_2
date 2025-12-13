# 🚀 Quick Start - 15 Minuten Setup

Schnellanleitung für die Inbetriebnahme von InfluxDB + Grafana auf deiner QNAP TS-210.

## ✅ Checkliste

- [ ] QNAP Container Station installiert
- [ ] Netzwerk-IP deiner QNAP bekannt (z.B. 192.168.1.100)
- [ ] ESP32 mit Arduino IDE verbunden
- [ ] InfluxDB Client Library installiert

---

## 📦 Schritt 1: QNAP vorbereiten (5 Min)

### 1.1 Container Station installieren

1. QNAP öffnen im Browser
2. App Center → "Container Station" suchen
3. Installieren
4. Warten bis fertig

### 1.2 Ordner erstellen

1. File Station öffnen
2. Navigiere zu: `/Container/`
3. Neuer Ordner: `sensor-logging`

### 1.3 Dateien hochladen

**Per File Station:**
- `docker-compose.yml` hochladen
- `grafana_dashboard.json` hochladen

**Oder per USB-Stick:**
- Dateien auf USB-Stick kopieren
- USB-Stick an QNAP anschließen
- Dateien nach `/Container/sensor-logging/` kopieren

---

## 🐳 Schritt 2: Docker Container starten (3 Min)

### 2.1 Passwörter ändern

**WICHTIG!** Öffne `docker-compose.yml` im Text-Editor:

```yaml
# Zeile 17 - InfluxDB Admin Passwort
DOCKER_INFLUXDB_INIT_PASSWORD=MeinSicheresPW123!

# Zeile 20 - InfluxDB API Token
DOCKER_INFLUXDB_INIT_ADMIN_TOKEN=SuperGeheimesToken456!

# Zeile 30 - Grafana Admin Passwort
GF_SECURITY_ADMIN_PASSWORD=GrafanaPW789!
```

Speichern!

### 2.2 Container starten

**Container Station GUI:**

1. Container Station → Create → Create Application
2. Name: `sensor-logging`
3. `docker-compose.yml` auswählen
4. **Create** klicken
5. Warten (~2 Minuten beim ersten Mal)

**Status prüfen:**
- Container Station → Container
- `influxdb` sollte grün sein
- `grafana` sollte grün sein

---

## 🔑 Schritt 3: InfluxDB Token holen (2 Min)

### 3.1 InfluxDB öffnen

Browser:
```
http://DEINE-QNAP-IP:8086
```

Beispiel:
```
http://192.168.1.100:8086
```

### 3.2 Einloggen

- Username: `admin`
- Password: (was du in Zeile 17 gesetzt hast)

### 3.3 Token kopieren

1. Links: **Data** (💾)
2. Tab: **API Tokens**
3. `admin's Token` anklicken
4. **Copy to Clipboard** klicken
5. Token in Notepad einfügen (wird gleich gebraucht!)

---

## 📊 Schritt 4: Grafana einrichten (3 Min)

### 4.1 Grafana öffnen

Browser:
```
http://DEINE-QNAP-IP:3000
```

### 4.2 Einloggen

- Username: `admin`
- Password: (was du in Zeile 30 gesetzt hast)

### 4.3 Data Source hinzufügen

1. Links: **⚙️ Configuration** → **Data Sources**
2. **Add data source**
3. **InfluxDB** auswählen

**Einstellungen:**
```
Name: InfluxDB Sensors
Query Language: Flux
URL: http://influxdb:8086
Organization: home
Token: [HIER DEIN TOKEN AUS 3.3 EINFÜGEN]
Default Bucket: sensors
```

4. **Save & Test** → Sollte grünes ✓ zeigen

### 4.4 Dashboard importieren

1. Links: **+** → **Import**
2. **Upload JSON file**
3. `grafana_dashboard.json` auswählen
4. Data Source: **InfluxDB Sensors** auswählen
5. **Import**

Dashboard ist fertig! (Zeigt noch keine Daten, normal)

---

## 🔧 Schritt 5: ESP32 konfigurieren (2 Min)

### 5.1 Library installieren

Arduino IDE:
1. **Sketch** → **Include Library** → **Manage Libraries**
2. Suche: `ESP8266 Influxdb`
3. Installiere: **ESP8266 Influxdb by Tobias Schürg**

### 5.2 Code anpassen

Öffne: `CYD_I2C_Master.ino`

**Zeile 67 prüfen:**
```cpp
#define ENABLE_INFLUXDB  // Sollte NICHT auskommentiert sein
```

**Zeile 72 - QNAP IP eintragen:**
```cpp
#define INFLUXDB_URL "http://192.168.1.100:8086"
                              ↑↑↑↑↑↑↑↑↑↑↑↑
                          DEINE QNAP IP
```

**Zeile 77 - Token eintragen:**
```cpp
#define INFLUXDB_TOKEN "IHR-TOKEN-AUS-SCHRITT-3.3"
```

**Speichern!**

### 5.3 Hochladen

1. ESP32 per USB verbinden
2. **Upload** (→) klicken
3. Warten bis fertig

### 5.4 Testen

**Serial Monitor öffnen (115200 baud):**

Sollte zeigen:
```
[WiFi] Connected!
[WiFi] IP: 192.168.1.XXX
[InfluxDB] Initializing...
[InfluxDB] Connected to: http://192.168.1.100:8086
[SD] Card Size: 16384 MB
[READY] System running!
```

Nach ~60 Sekunden:
```
[SD] Indoor data logged to /20250112_indoor.csv
[InfluxDB] Indoor data written
```

✅ **Perfekt!**

---

## 🎉 Fertig!

### Grafana Dashboard ansehen

1. Browser: `http://DEINE-QNAP-IP:3000`
2. Dashboard: **ESP32 Sensor Dashboard**
3. Zeitbereich: **Last 1 hour**
4. Nach ~1 Minute sollten erste Daten erscheinen

### Webserver (CSV Download)

1. Browser: `http://ESP32-IP` (IP aus Serial Monitor)
2. CSV-Dateien herunterladen

---

## 🛠️ Troubleshooting

### ESP32 verbindet nicht zu InfluxDB

**Serial Monitor zeigt:**
```
[InfluxDB] Connection failed: ...
```

**Lösung:**
1. Token nochmal prüfen (Copy-Paste Fehler?)
2. QNAP IP korrekt? Ping-Test:
   ```
   ping 192.168.1.100
   ```
3. Port 8086 erreichbar?
   ```
   curl http://192.168.1.100:8086/health
   ```

### Grafana zeigt keine Daten

**Prüfen:**
1. InfluxDB Data Explorer öffnen
2. Bucket: `sensors`
3. Measurement: `indoor_sensor`
4. Sind Daten da?

**Wenn leer:**
- ESP32 Serial Monitor prüfen
- `[InfluxDB] Indoor data written` sollte alle 60s kommen

### Container startet nicht

**Container Station → Log ansehen:**
```
docker-compose logs influxdb
```

**Häufig:**
- Ports bereits belegt (andere Software auf 8086?)
- Zu wenig RAM (andere Container stoppen)

---

## 📱 Nächste Schritte

### Dashboard anpassen

- Grafana → Dashboard → ⚙️ Settings
- Panels hinzufügen/bearbeiten
- Farben, Schwellwerte anpassen

### Alerts einrichten

- Grafana → Alerting
- Temperatur > 30°C → E-Mail
- Batterie < 2800mV → Warnung

### Mobile App

- Grafana App (iOS/Android)
- Server: `http://DEINE-QNAP-IP:3000`
- Unterwegs Dashboard ansehen

---

## ⏱️ Zusammenfassung

| Schritt | Zeit | Status |
|---------|------|--------|
| QNAP Container Station | 5 Min | ⬜ |
| Docker starten | 3 Min | ⬜ |
| InfluxDB Token | 2 Min | ⬜ |
| Grafana einrichten | 3 Min | ⬜ |
| ESP32 konfigurieren | 2 Min | ⬜ |
| **GESAMT** | **~15 Min** | |

**Viel Erfolg! 🚀**

Bei Problemen → siehe `README.md` für Details
