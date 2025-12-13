# InfluxDB Alternativen für ältere QNAP NAS-Systeme

## ⚠️ QNAP TS-210 hat keine Container Station

Die **QNAP TS-210** (und ähnlich alte Modelle) unterstützen keine Container Station:
- Zu alte CPU-Architektur
- Zu wenig RAM für Docker
- QTS-Version zu alt

## ✅ Empfohlene Lösungen

---

### **Option 1: Nur CSV-Logging (EINFACHSTE LÖSUNG)**

**Status:** ✅ Bereits fertig implementiert!

Das System funktioniert perfekt **ohne** InfluxDB:

**Features:**
- ✅ SD-Karten-Logging (alle 15 Min)
- ✅ Monatliche CSV-Dateien (202501_indoor.csv)
- ✅ Webserver für Download
- ✅ Display zeigt Echtzeit-Daten
- ✅ 100% offline-fähig

**Aktivieren:**
```cpp
// In CYD_I2C_Master.ino Zeile 67:
// #define ENABLE_INFLUXDB  ← Auskommentiert lassen
```

**CSV-Dateien auswerten:**
- Webserver: `http://ESP32-IP` → Download
- Excel/LibreOffice öffnen
- Graphen erstellen

**Vorteile:**
- Keine externe Infrastruktur
- Keine laufenden Kosten
- Langzeit-Archivierung auf SD-Karte
- Volle Kontrolle über Daten

---

### **Option 2: InfluxDB Cloud (KOSTENLOS)**

**URL:** https://cloud2.influxdata.com/signup

**Free Tier:**
- ✅ Keine Kreditkarte nötig
- ✅ 30 Tage Daten-Retention
- ✅ 10 MB Upload/Tag (ausreichend!)
- ✅ Von überall erreichbar
- ✅ Grafana Cloud Integration

**Setup (5 Minuten):**

1. **Account erstellen:**
   - https://cloud2.influxdata.com/signup
   - Region: EU Central (Frankfurt)

2. **Bucket erstellen:**
   - Name: `sensors`
   - Retention: 30 days

3. **API Token kopieren:**
   - Load Data → API Tokens → Generate Token
   - "All Access" auswählen

4. **ESP32 konfigurieren:**
   ```cpp
   // Zeile 67: Aktivieren
   #define ENABLE_INFLUXDB

   // Zeile 72: Cloud URL
   #define INFLUXDB_URL "https://eu-central-1-1.aws.cloud2.influxdata.com"

   // Zeile 77: Dein Token
   #define INFLUXDB_TOKEN "IHR_CLOUD_TOKEN_HIER"

   // Zeile 82: Deine Email als Org
   #define INFLUXDB_ORG "deine.email@beispiel.de"
   ```

5. **Upload auf ESP32**

**Grafana Cloud verbinden:**
- https://grafana.com/auth/sign-up
- Data Source → InfluxDB
- URL: `https://eu-central-1-1.aws.cloud2.influxdata.com`
- Token: (gleicher wie oben)

**Einschränkungen:**
- Nur 30 Tage Daten (dann gelöscht)
- Rate Limits bei vielen Sensoren
- Internet-Abhängigkeit

**Ideal für:**
- Live-Monitoring
- Grafana-Dashboards
- Remote-Zugriff

➡️ **CSV auf SD-Karte als Langzeit-Backup behalten!**

---

### **Option 3: Alter PC/Laptop als Server**

Falls du einen alten PC/Laptop hast:

**Windows 10/11:**

```powershell
# Docker Desktop installieren
# https://www.docker.com/products/docker-desktop/

# Danach:
docker run -d -p 8086:8086 ^
  -v c:\influxdb\data:/var/lib/influxdb2 ^
  influxdb:2.7

docker run -d -p 3000:3000 ^
  -v c:\grafana\data:/var/lib/grafana ^
  grafana/grafana
```

**Linux (Ubuntu/Debian):**

```bash
# Docker installieren
sudo apt update
sudo apt install docker.io docker-compose

# Dateien kopieren
cd /home/user/influxdb-setup/
# (docker-compose.yml aus diesem Projekt nutzen)

# Starten
docker-compose up -d
```

**Vorteile:**
- Volle Kontrolle
- Unbegrenzte Daten
- Kein Internet nötig
- Kostenlos

**Nachteile:**
- PC muss immer laufen
- Stromverbrauch (~20-50W)

---

### **Option 4: Raspberry Pi**

**Empfohlen:** Raspberry Pi 3B+ oder 4 (ab 2GB RAM)

**Installation:**

```bash
# System aktualisieren
sudo apt update && sudo apt upgrade -y

# Docker installieren
curl -fsSL https://get.docker.com -o get-docker.sh
sudo sh get-docker.sh
sudo usermod -aG docker pi

# Docker Compose installieren
sudo apt install docker-compose

# InfluxDB Setup-Dateien kopieren
mkdir ~/influxdb-setup
cd ~/influxdb-setup
# docker-compose.yml hierher kopieren

# Starten
docker-compose up -d

# Status prüfen
docker-compose ps
```

**InfluxDB UI öffnen:**
```
http://RASPBERRY-PI-IP:8086
```

**Vorteile:**
- Niedriger Stromverbrauch (~5W)
- Günstig (~50€)
- Perfekt für Heimnetzwerk
- 24/7 Betrieb

**Raspberry Pi 3B+ Ressourcen:**
- RAM: 1GB (ausreichend)
- CPU: 4× ARM Cortex-A53
- Stromaufnahme: ~5W
- Speicher: MicroSD (32GB empfohlen)

➡️ **Beste Balance aus Preis/Leistung/Stromverbrauch**

---

### **Option 5: NAS-Alternative (Synology, QNAP neuere Modelle)**

Falls du später upgraden willst:

**QNAP:**
- TS-x53D Serie oder neuer
- Container Station Support
- 2+ GB RAM

**Synology:**
- DS220+ oder neuer
- Docker Package verfügbar
- 2+ GB RAM

---

## 📊 Vergleich

| Lösung | Kosten | Setup | Langzeit-Daten | Grafana |
|--------|--------|-------|----------------|---------|
| **CSV-Only** | 0€ | 0 Min | ✅ Unbegrenzt | ❌ Nein |
| **InfluxDB Cloud** | 0€ | 5 Min | ⚠️ 30 Tage | ✅ Ja |
| **Alter PC** | 0€ | 10 Min | ✅ Unbegrenzt | ✅ Ja |
| **Raspberry Pi** | ~50€ | 15 Min | ✅ Unbegrenzt | ✅ Ja |
| **Neue QNAP/Synology** | ~300€ | 10 Min | ✅ Unbegrenzt | ✅ Ja |

---

## 🎯 Empfehlung für dich

### **Sofort:**
```cpp
// CSV-Only nutzen (bereits fertig!)
// #define ENABLE_INFLUXDB  // Auskommentiert
```

**Du hast:**
- Monatliche Logs auf SD-Karte
- Webserver für Download
- Alles funktioniert

---

### **Optional später:**

**Falls du Grafana-Dashboards willst:**

**Budget-Lösung:**
→ InfluxDB Cloud (kostenlos, 5 Min Setup)

**Beste Lösung:**
→ Raspberry Pi 3B+ (~50€, einmalig)

**Premium-Lösung:**
→ Neue QNAP/Synology (~300€+)

---

## 💾 Hybrid-Ansatz (EMPFOHLEN)

```
┌─────────────────────────────────────┐
│         ESP32 (CYD)                 │
│                                     │
│  Indoor/Outdoor Sensoren            │
└──────────────┬──────────────────────┘
               │
               ↓
     ┌─────────┴─────────┐
     ↓                   ↓
┌──────────┐      ┌─────────────┐
│ SD-Karte │      │  InfluxDB   │
│ (Backup) │      │   (Cloud)   │
│          │      │             │
│ CSV      │      │  Grafana    │
│ Langzeit │      │  Live       │
└──────────┘      └─────────────┘
```

**Vorteil:**
- CSV = Langzeit-Archiv (Jahre, offline)
- InfluxDB = Live-Dashboards (30 Tage)
- Redundanz bei Ausfall

---

## 🔧 Code-Änderungen

**CSV-Only aktivieren:**
```cpp
// Zeile 67
// #define ENABLE_INFLUXDB  // Auskommentiert
```

**InfluxDB Cloud aktivieren:**
```cpp
// Zeile 67
#define ENABLE_INFLUXDB  // Aktiviert

// Zeile 72
#define INFLUXDB_URL "https://eu-central-1-1.aws.cloud2.influxdata.com"

// Zeile 77
#define INFLUXDB_TOKEN "dein-cloud-token"

// Zeile 82
#define INFLUXDB_ORG "deine.email@example.com"
```

---

## 📞 Support

**CSV-Dateien öffnen in Excel:**
1. Excel → Daten → Aus Text/CSV
2. Datei auswählen
3. Trennzeichen: Komma
4. Importieren

**Grafana Dashboard erstellen (Cloud):**
1. https://grafana.com
2. Data Source: InfluxDB Cloud
3. Dashboard Import: `grafana_dashboard.json`

---

**Viel Erfolg! 🎉**

Die CSV-Lösung funktioniert bereits perfekt für deine TS-210!
