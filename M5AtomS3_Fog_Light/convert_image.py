#!/usr/bin/env python3
"""
Konvertiert ein Bild in ein 1-Bit Bitmap-Array für Arduino
"""

from PIL import Image
import sys

def image_to_1bit_array(image_path, output_size=96, threshold=128):
    """
    Konvertiert ein Bild in ein 1-Bit Bitmap-Array

    Args:
        image_path: Pfad zum Eingabebild
        output_size: Zielgröße (quadratisch)
        threshold: Schwellwert für Schwarz/Weiß (0-255)
    """
    # Bild laden
    img = Image.open(image_path)

    # In Graustufen konvertieren
    img = img.convert('L')

    # Größe anpassen (mit Antialiasing)
    img = img.resize((output_size, output_size), Image.Resampling.LANCZOS)

    # Optional: Bild speichern zur Überprüfung
    img.save(image_path.replace('.png', '_resized.png'))

    # In 1-Bit konvertieren (Schwellwert)
    pixels = img.load()
    width, height = img.size

    # Bitmap-Array erstellen
    bitmap = []
    for y in range(height):
        row_bits = 0
        for x in range(width):
            pixel_value = pixels[x, y]
            # Invertieren: Weiß (255) = 0, Schwarz (0) = 1
            # Aber das Symbol ist gelb/orange, also hell
            # Wir wollen das Symbol (helle Pixel) als 1 setzen
            if pixel_value > threshold:
                row_bits |= (1 << x)
        bitmap.append(row_bits)

    # Alternative: Byte-Array für einfachere Verwendung
    # Jedes Byte speichert 8 Pixel
    byte_array = []
    for y in range(height):
        for x in range(0, width, 8):
            byte_val = 0
            for bit in range(8):
                if x + bit < width:
                    pixel_value = pixels[x + bit, y]
                    if pixel_value > threshold:
                        byte_val |= (1 << (7 - bit))
            byte_array.append(byte_val)

    # C-Array Code generieren
    print(f"// Fog light icon: {width}x{height} pixels, 1-bit bitmap")
    print(f"const unsigned char fog_light_icon_{width}x{height}[] = {{")

    bytes_per_row = (width + 7) // 8
    for y in range(height):
        row_bytes = byte_array[y * bytes_per_row:(y + 1) * bytes_per_row]
        hex_values = ', '.join([f"0x{b:02X}" for b in row_bytes])
        print(f"  {hex_values},  // Row {y}")

    print("};")
    print(f"\n// Total size: {len(byte_array)} bytes")
    print(f"// Width: {width}, Height: {height}")
    print(f"// Bytes per row: {bytes_per_row}")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python3 convert_image.py <image_path> [size] [threshold]")
        sys.exit(1)

    image_path = sys.argv[1]
    size = int(sys.argv[2]) if len(sys.argv) > 2 else 96
    threshold = int(sys.argv[3]) if len(sys.argv) > 3 else 128

    image_to_1bit_array(image_path, size, threshold)
