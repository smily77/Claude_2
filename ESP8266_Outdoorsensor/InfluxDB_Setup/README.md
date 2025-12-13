# InfluxDB + Grafana Setup für ESP32 Sensor-Logging

⚠️ **WICHTIG: QNAP TS-210 wird NICHT unterstützt!**

Die **TS-210** ist zu alt für Container Station. Siehe `ALTERNATIVEN.md` für:
- CSV-Only Lösung (bereits fertig!)
- InfluxDB Cloud (kostenlos)
- Raspberry Pi Setup
- Alter PC als Server

---

Komplette Anleitung zum Einrichten der Datenbank auf **neueren QNAP Modellen** (mit Container Station Support).

## 📋 Übersicht

```
ESP32 (CYD) → WiFi → InfluxDB → Grafana Dashboard
     ↓
  SD-Karte (CSV Backup)
```

## 🔧 Voraussetzungen

### QNAP TS-210 Vorbereitung

1. **Container Station installieren**
   - QNAP App Center öffnen
   - "Container Station" suchen
   - Installieren und starten

2. **SSH aktivieren** (optional, für erweiterte Konfiguration)
   - QNAP Admin → Systemeinstellungen → Telnet / SSH
   - SSH aktivieren (Port 22)

## 🚀 Installation

### Schritt 1: Dateien auf QNAP hochladen

1. Im QNAP File Station einen Ordner erstellen:
   ```
   /share/Container/sensor-logging/
   ```

2. Dateien hochladen:
   - `docker-compose.yml`
   - `README.md` (diese Datei)
   - `grafana_dashboard.json` (später)

### Schritt 2: Container Station Setup

**Option A: Über GUI (einfacher)**

1. Container Station öffnen
2. "Create" → "Create Application"
3. Name: `sensor-logging`
4. `docker-compose.yml` auswählen
5. **WICHTIG: Passwörter ändern!**
   - Zeile 17: `DOCKER_INFLUXDB_INIT_PASSWORD=DEIN_PASSWORT`
   - Zeile 20: `DOCKER_INFLUXDB_INIT_ADMIN_TOKEN=DEIN_TOKEN`
   - Zeile 30: `GF_SECURITY_ADMIN_PASSWORD=DEIN_PASSWORT`
6. "Create" klicken

**Option B: Über SSH (für Fortgeschrittene)**

```bash
# Via SSH auf QNAP einloggen
ssh admin@192.168.1.XXX

# In Ordner wechseln
cd /share/Container/sensor-logging/

# Container starten
docker-compose up -d

# Status prüfen
docker-compose ps
```

### Schritt 3: InfluxDB konfigurieren

1. **InfluxDB UI öffnen**
   ```
   http://QNAP-IP:8086
   ```

2. **Login** (bei Erststart automatisch eingerichtet)
   - Username: `admin`
   - Password: (was du in docker-compose.yml gesetzt hast)

3. **API Token kopieren**
   - Links: Data → API Tokens
   - Token für "admin" anklicken
   - Kopieren (benötigt für ESP32)

4. **Bucket prüfen**
   - Links: Load Data → Buckets
   - `sensors` sollte existieren (automatisch erstellt)

### Schritt 4: Grafana konfigurieren

1. **Grafana öffnen**
   ```
   http://QNAP-IP:3000
   ```

2. **Login**
   - Username: `admin`
   - Password: (was du in docker-compose.yml gesetzt hast)

3. **InfluxDB als Data Source hinzufügen**
   - Links: Configuration (⚙️) → Data Sources
   - "Add data source" → InfluxDB

   **Einstellungen:**
   ```
   Name: InfluxDB Sensors
   Query Language: Flux
   URL: http://influxdb:8086
   Organization: home
   Token: [Dein Token aus Schritt 3.3]
   Default Bucket: sensors
   ```

4. **"Save & Test"** - sollte grünes ✓ zeigen

### Schritt 5: ESP32 konfigurieren

1. **Arduino IDE öffnen**

2. **Bibliothek installieren**
   - Sketch → Include Library → Manage Libraries
   - Suche: `ESP8266 Influxdb`
   - Installiere: **InfluxDB Client for Arduino** von Tobias Schürg

3. **CYD_I2C_Master.ino bearbeiten**

   ```cpp
   // Zeile 67: InfluxDB aktiviert lassen
   #define ENABLE_INFLUXDB

   // Zeile 72: QNAP IP-Adresse eintragen
   #define INFLUXDB_URL "http://192.168.1.XXX:8086"

   // Zeile 77: Token aus InfluxDB eintragen
   #define INFLUXDB_TOKEN "dein-token-hier"
   ```

4. **Hochladen auf ESP32**

5. **Serial Monitor prüfen** (115200 baud)
   ```
   [InfluxDB] Initializing...
   [InfluxDB] Connected to: http://192.168.1.XXX:8086
   [InfluxDB] Indoor data written
   ```

## 📊 Grafana Dashboard importieren

1. **Dashboard JSON importieren**
   - Grafana → Dashboards → Import
   - `grafana_dashboard.json` hochladen
   - Data Source: "InfluxDB Sensors" auswählen
   - Import

2. **Dashboard ansehen**
   - Sollte sofort Daten zeigen (nach ersten ESP32-Logs)

## 🧪 Test

1. **Manuelle Abfrage in InfluxDB**
   - InfluxDB UI → Data Explorer
   - Bucket: `sensors`
   - Measurement: `indoor_sensor` auswählen
   - Fields: `temperature` auswählen
   - Submit

2. **Grafana Test**
   - Dashboard öffnen
   - Zeitbereich: "Last 1 hour"
   - Refresh klicken

## 📁 Datenverzeichnisse

Nach dem Start werden folgende Ordner erstellt:

```
/share/Container/sensor-logging/
├── docker-compose.yml
├── influxdb/
│   ├── data/          # InfluxDB Datenbank
│   └── config/        # InfluxDB Konfiguration
└── grafana/
    ├── data/          # Grafana Dashboards & Einstellungen
    └── provisioning/  # Auto-Konfiguration
```

## 🛠️ Wartung

### Container neu starten
```bash
cd /share/Container/sensor-logging/
docker-compose restart
```

### Logs ansehen
```bash
docker-compose logs -f influxdb
docker-compose logs -f grafana
```

### Container stoppen
```bash
docker-compose down
```

### Container löschen (Daten bleiben!)
```bash
docker-compose down
# Daten löschen:
rm -rf influxdb/ grafana/
```

## 🔐 Sicherheit

### Standard-Passwörter ändern

**WICHTIG:** Ändere alle Passwörter in `docker-compose.yml`:

```yaml
# InfluxDB
DOCKER_INFLUXDB_INIT_PASSWORD=STARKES_PASSWORT_123!
DOCKER_INFLUXDB_INIT_ADMIN_TOKEN=LANGER_ZUFALLIGER_TOKEN_456!

# Grafana
GF_SECURITY_ADMIN_PASSWORD=ANDERES_PASSWORT_789!
```

### Firewall (optional)

Wenn QNAP Firewall aktiv:
- Port 8086 (InfluxDB) nur im lokalen Netzwerk öffnen
- Port 3000 (Grafana) nur im lokalen Netzwerk öffnen

## ❓ Troubleshooting

### Container startet nicht

```bash
docker-compose logs influxdb
```

**Lösung:** Ports bereits belegt?
```bash
netstat -tuln | grep 8086
```

### ESP32 kann nicht zu InfluxDB verbinden

1. **Ping-Test:**
   ```bash
   ping 192.168.1.XXX  # QNAP IP
   ```

2. **Port-Test:**
   ```bash
   curl http://192.168.1.XXX:8086/health
   ```

   Sollte antworten:
   ```json
   {"status":"pass"}
   ```

3. **Token prüfen:**
   - InfluxDB UI → API Tokens
   - Token kopieren und in ESP32-Code einfügen

### Grafana zeigt keine Daten

1. **Data Source testen:**
   - Settings → Data Sources → InfluxDB Sensors
   - "Save & Test" → sollte grün sein

2. **Manuelle Query:**
   - Explore → InfluxDB Sensors
   - Query Builder benutzen

### QNAP Ressourcen-Probleme

**TS-210 ist etwas älter, daher:**

1. **Nur notwendige Container laufen lassen**
2. **RAM prüfen:**
   ```bash
   free -h
   ```
3. **Retention-Zeit reduzieren:**
   ```yaml
   DOCKER_INFLUXDB_INIT_RETENTION=90d  # Nur 90 Tage statt 365
   ```

## 📈 Datenvolumen

**Beispiel-Rechnung:**

- 2 Sensoren (Indoor + Outdoor)
- Logging alle 60 Sekunden
- ~10 Felder pro Messung

**Pro Tag:** ~2.5 MB
**Pro Monat:** ~75 MB
**Pro Jahr:** ~900 MB

➡️ Für TS-210 kein Problem!

## 🌐 Zugriff von außen (optional)

### Via myQNAPcloud

1. QNAP Admin → myQNAPcloud
2. Port-Weiterleitung für 3000 (Grafana)
3. **WICHTIG:** Starke Passwörter + 2FA aktivieren!

### Via VPN (sicherer)

1. QNAP VPN Server installieren
2. OpenVPN/WireGuard konfigurieren
3. Von unterwegs per VPN verbinden
4. Dann auf http://192.168.1.XXX:3000

## 📞 Support

Bei Problemen:

1. **Logs prüfen:**
   ```bash
   docker-compose logs
   ```

2. **ESP32 Serial Monitor:**
   - Fehlermeldungen kopieren

3. **InfluxDB Health Check:**
   ```bash
   curl http://localhost:8086/health
   ```

## 🔄 Updates

### InfluxDB Update
```bash
docker-compose pull influxdb
docker-compose up -d influxdb
```

### Grafana Update
```bash
docker-compose pull grafana
docker-compose up -d grafana
```

---

**Viel Erfolg! 🎉**

Bei Fragen zur Konfiguration, siehe die Kommentare im Code oder die Original-Dokumentation:
- InfluxDB: https://docs.influxdata.com/
- Grafana: https://grafana.com/docs/
