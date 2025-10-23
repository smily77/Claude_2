# Eigene Bilder auf dem M5Stack anzeigen

## Schritt-für-Schritt Anleitung

### 1. Bild herunterladen oder erstellen
- Suche ein Bild im Internet (z.B. Weltkugel, Logo, Icon)
- Achte auf die Lizenz! Verwende lizenzfreie Bilder von:
  - **Pixabay**: https://pixabay.com/
  - **Unsplash**: https://unsplash.com/
  - **Flaticon**: https://www.flaticon.com/ (für Icons)

### 2. Bild vorbereiten

#### Option A: Online mit GIMP oder Photoshop
1. Öffne das Bild in einem Bildbearbeitungsprogramm
2. Größe ändern auf **80x80 Pixel** (oder andere gewünschte Größe)
3. Als **JPEG** speichern (Qualität 80-90%)

#### Option B: Online mit ImageResizer
1. Gehe zu https://imageresizer.com/
2. Lade dein Bild hoch
3. Größe ändern auf 80x80 Pixel
4. Download als JPEG

### 3. Bild in C-Array konvertieren

#### Online-Tool (empfohlen)
1. Öffne https://notisrac.github.io/FileToCArray/
2. Wähle dein JPEG-Bild aus
3. Klicke auf "Convert"
4. Kopiere das generierte C-Array

#### Alternatives Tool
- https://tomeko.net/online_tools/file_to_hex.php?lang=en

### 4. Array in den Code einfügen

Öffne die Datei `image_data.h` und ersetze das Array:

```cpp
const unsigned char mein_bild[] PROGMEM = {
  // Hier das kopierte Array einfügen
  0xFF, 0xD8, 0xFF, 0xE0, ...
};

const unsigned int mein_bild_len = 1234; // Größe anpassen!
```

### 5. Code anpassen

In `M5_Hello_World.ino` ändere die Zeile:

```cpp
// Vorher:
M5.Display.drawJpg(globe_icon_80x80, globe_icon_80x80_len, 20, 5, 80, 80);

// Nachher:
M5.Display.drawJpg(mein_bild, mein_bild_len, 20, 5, 80, 80);
```

## Tipps und Tricks

### Bildgröße optimieren
- **Kleine Bilder**: 40x40 bis 80x80 Pixel (Icons, Logos)
- **Mittlere Bilder**: 100x100 bis 150x150 Pixel
- **Große Bilder**: 240x135 oder 320x240 Pixel (Fullscreen)
- M5Stack Core2 Display: **320x240 Pixel**

### Speicher sparen
- JPEG ist komprimierter als PNG (kleinere Dateigröße)
- Reduziere die JPEG-Qualität auf 70-80%
- Kleinere Bilder = weniger Speicher
- ESP32 hat ca. 320KB RAM - große Bilder können Probleme machen

### Mehrere Bilder verwenden
In `image_data.h`:
```cpp
const unsigned char bild1[] PROGMEM = { ... };
const unsigned int bild1_len = 1000;

const unsigned char bild2[] PROGMEM = { ... };
const unsigned int bild2_len = 1500;

const unsigned char bild3[] PROGMEM = { ... };
const unsigned int bild3_len = 800;
```

### Bilder positionieren
```cpp
// Syntax: drawJpg(array, länge, x, y, breite, höhe)
M5.Display.drawJpg(bild, bild_len, 0, 0, 80, 80);      // Oben links
M5.Display.drawJpg(bild, bild_len, 120, 80, 80, 80);   // Zentriert
M5.Display.drawJpg(bild, bild_len, 240, 0, 80, 80);    // Oben rechts
M5.Display.drawJpg(bild, bild_len, 0, 160, 80, 80);    // Unten links
```

## Beispiel-Quellen für Bilder

### Weltkugel/Erde
- Pixabay: https://pixabay.com/de/images/search/earth%20globe/
- Flaticon: https://www.flaticon.com/search?word=globe

### Logos und Icons
- Flaticon: https://www.flaticon.com/
- Icons8: https://icons8.com/

### Fotos
- Unsplash: https://unsplash.com/
- Pexels: https://www.pexels.com/

## Fehlerbehebung

### "Bild wird nicht angezeigt"
- Überprüfe, ob das Array-Name im Code übereinstimmt
- Stelle sicher, dass die Länge (_len) korrekt ist
- Prüfe, ob das Bild als JPEG gespeichert wurde

### "Compiler-Fehler: Array zu groß"
- Reduziere die Bildgröße
- Komprimiere das JPEG stärker (niedrigere Qualität)
- Verwende SD-Karte für große Bilder

### "Display zeigt verzerrtes Bild"
- Überprüfe Breite und Höhe in drawJpg()
- Stelle sicher, dass das Seitenverhältnis stimmt

## Alternative: Bilder von SD-Karte laden

Wenn du viele oder große Bilder hast, verwende eine SD-Karte:

```cpp
#include <SD.h>

void setup() {
  M5.begin();
  SD.begin();

  // Bild von SD-Karte laden
  M5.Display.drawJpgFile(SD, "/bild.jpg", 0, 0, 320, 240);
}
```

Vorteile:
- Unbegrenzt viele Bilder
- Größere Bilder möglich
- Einfacher zu aktualisieren

Nachteile:
- SD-Karte erforderlich
- Langsamer als eingebettete Bilder
