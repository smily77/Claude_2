# ESP32 Hand Compass with BNO055

A professional hand compass application for ESP32/ESP32-S3 using the Bosch BNO055 9-axis IMU sensor and LovyanGFX display library.

![ESP32 Hand Compass](docs/compass-preview.png)

## Features

- **Beautiful Visual Interface**
  - Compass rose with rotating needle
  - Digital heading display (0-360°)
  - Cardinal direction text (N, NE, E, SE, S, SW, W, NW)
  - Real-time calibration status panel

- **Smart Calibration System**
  - Automatic calibration on first use
  - Calibration data stored in NVS (non-volatile storage)
  - Manual calibration mode via touch button
  - Visual calibration instructions
  - Warning indicators for poor calibration

- **Advanced Sensor Management**
  - Exponential smoothing filter for stable heading
  - Wrap-around handling for 0°/360° transition
  - State machine for calibration management
  - Robust I2C communication

- **Hardware Support**
  - ESP32 and ESP32-S3 compatible
  - Flexible I2C pin configuration
  - Touch-enabled TFT displays
  - 320x280 resolution (configurable)

## Hardware Requirements

### Required Components

1. **Microcontroller**
   - ESP32 or ESP32-S3 development board

2. **IMU Sensor**
   - Bosch BNO055 9-axis IMU module
   - I2C interface

3. **Display**
   - TFT LCD with touch support
   - Recommended: 320x280 or similar resolution
   - Examples: CYD (Cheap Yellow Display), ILI9341, ST7789

### Wiring

#### BNO055 Connection (I2C)

| BNO055 Pin | ESP32 Pin | ESP32-S3 Pin | Notes |
|------------|-----------|--------------|-------|
| VCC | 3.3V | 3.3V | Power |
| GND | GND | GND | Ground |
| SDA | GPIO 21 | GPIO 8 | I2C Data (configurable) |
| SCL | GPIO 22 | GPIO 9 | I2C Clock (configurable) |

**Note:** You can override the default I2C pins by defining `extSDA` and `extSCL` in your build configuration.

#### Display Connection

Display wiring depends on your specific hardware. Please refer to your display module's documentation.

## Software Requirements

### Arduino IDE Setup

1. Install [Arduino IDE](https://www.arduino.cc/en/software) (version 1.8.19 or newer, or Arduino IDE 2.x)

2. Add ESP32 board support:
   - Open Arduino IDE
   - Go to **File > Preferences**
   - Add to "Additional Board Manager URLs":
     ```
     https://espressif.github.io/arduino-esp32/package_esp32_index.json
     ```
   - Go to **Tools > Board > Board Manager**
   - Search for "esp32" and install "esp32 by Espressif Systems"

### Required Libraries

Install these libraries via **Library Manager** (Sketch > Include Library > Manage Libraries):

1. **LovyanGFX** by lovyan03
   - Fast graphics library for ESP32 TFT displays
   - Search: "LovyanGFX"

2. **Adafruit BNO055** by Adafruit
   - BNO055 sensor driver
   - Search: "Adafruit BNO055"

3. **Adafruit Unified Sensor** by Adafruit
   - Required dependency for BNO055
   - Search: "Adafruit Unified Sensor"

## Installation

1. **Clone the repository**
   ```bash
   git clone https://github.com/yourusername/esp32-bno055-hand-compass.git
   cd esp32-bno055-hand-compass
   ```

2. **Configure your display**

   **IMPORTANT:** You must provide your own display configuration!

   - Edit `src/CYD_Display_Config.h`
   - Replace the placeholder with your actual LovyanGFX configuration
   - Refer to LovyanGFX examples for your specific display module

   Example configurations can be found in:
   - LovyanGFX library examples
   - [LovyanGFX GitHub](https://github.com/lovyan03/LovyanGFX)

3. **Open in Arduino IDE**
   - Open `src/HandCompass_BNO055.ino` in Arduino IDE

4. **Select your board**
   - Go to **Tools > Board**
   - Select your ESP32 or ESP32-S3 board

5. **Upload**
   - Connect your ESP32 via USB
   - Click **Upload**

## Usage

### First Run - Automatic Calibration

When you run the compass for the first time (or after clearing NVS), it will automatically enter calibration mode:

1. The display shows "AUTO-CALIBRATION"
2. Move the device slowly in a **figure-8 pattern**
3. Rotate around all three axes:
   - Roll (tilt left/right)
   - Pitch (tilt forward/backward)
   - Yaw (rotate horizontally)
4. Watch the calibration status panel:
   - SYS: System calibration (0-3)
   - GYR: Gyroscope (0-3)
   - ACC: Accelerometer (0-3)
   - MAG: Magnetometer (0-3)
5. When all values reach **3** and remain stable for 3 seconds, calibration is saved automatically

### Normal Operation

After successful calibration:
- The compass shows your current heading
- The needle points in the direction you're facing
- Digital display shows degrees and cardinal direction
- Calibration status is shown at the bottom

### Manual Calibration

If you need to recalibrate:
1. Tap the **CALIBRATE** button (top-left)
2. Follow the same figure-8 movement pattern
3. Calibration saves automatically when complete
4. Or tap **CANCEL** to exit without saving

### Calibration Status Indicators

- **Green (3)**: Fully calibrated - excellent
- **Yellow (1-2)**: Partially calibrated - acceptable
- **Red (0)**: Not calibrated - poor accuracy

### Warning System

If the compass detects poor calibration during normal use:
- A warning message appears: "! CALIBRATE"
- This only appears after 5 seconds of poor calibration
- Recalibrate if this warning persists

## Project Structure

```
esp32-bno055-hand-compass/
├── src/
│   ├── HandCompass_BNO055.ino    # Main Arduino sketch
│   ├── CompassUI.h                # UI interface header
│   ├── CompassUI.cpp              # UI implementation
│   ├── BNO055Manager.h            # Sensor manager header
│   ├── BNO055Manager.cpp          # Sensor manager implementation
│   └── CYD_Display_Config.h       # Display configuration (user-provided)
├── lib/                           # Additional libraries (empty)
├── .gitignore                     # Git ignore file
└── README.md                      # This file
```

## Configuration

### Custom I2C Pins

To use custom I2C pins, add these defines before compilation:

```cpp
#define extSDA 16  // Your SDA pin
#define extSCL 17  // Your SCL pin
```

You can add these in:
- Arduino IDE: Create a `config.h` and include it
- PlatformIO: Add to `platformio.ini` build flags:
  ```ini
  build_flags =
    -D extSDA=16
    -D extSCL=17
  ```

### Heading Filter Adjustment

The heading filter smoothness can be adjusted in `BNO055Manager.cpp`:

```cpp
#define HEADING_FILTER_ALPHA 0.10f  // Lower = smoother, Higher = more responsive
```

- Lower values (0.05): Very smooth, slower response
- Default (0.10): Balanced
- Higher values (0.20): More responsive, less smooth

## Troubleshooting

### "BNO055 not found" Error

**Possible causes:**
1. **Wrong I2C pins** - Check your wiring
2. **Loose connections** - Verify all connections are secure
3. **Wrong I2C address** - BNO055 uses 0x28 by default
4. **Power issue** - Ensure sensor gets 3.3V
5. **Faulty sensor** - Try with I2C scanner sketch

**Solutions:**
```cpp
// Add debug output to setup():
Wire.begin(SDA_PIN, SCL_PIN);
Wire.beginTransmission(0x28);
byte error = Wire.endTransmission();
Serial.printf("I2C test result: %d (0=success)\n", error);
```

### Calibration Won't Complete

**Tips:**
- Calibration requires movement - don't keep device still
- Magnetometer needs figure-8 patterns
- Move slowly and smoothly
- Avoid magnetic interference (speakers, magnets, metal objects)
- Try calibrating away from electronic devices

### Heading Jumps or Unstable

**Possible causes:**
1. Poor calibration - Recalibrate
2. Magnetic interference - Move away from metal/electronics
3. Filter too responsive - Decrease `HEADING_FILTER_ALPHA`

### Display Issues

1. **Check CYD_Display_Config.h** - Ensure proper configuration for your display
2. **Test with LovyanGFX examples** - Verify display works independently
3. **Check brightness** - Adjust in `setup()`: `lcd.setBrightness(200);`

## Advanced Features

### NVS Storage

Calibration data is stored in ESP32's Non-Volatile Storage:
- Namespace: `"bno055cal"`
- Persists across reboots
- Clear NVS to force recalibration:
  ```cpp
  #include <Preferences.h>
  Preferences prefs;
  prefs.begin("bno055cal", false);
  prefs.clear();
  prefs.end();
  ```

### State Machine

The system uses three states:
1. **STATE_NORMAL** - Normal compass operation
2. **STATE_AUTO_CALIBRATING** - Auto-calibration on first boot
3. **STATE_MANUAL_CALIBRATING** - User-initiated calibration

## Contributing

Contributions are welcome! Please:
1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Test thoroughly
5. Submit a pull request

## License

This project is open-source and available under the MIT License.

## Credits

- **LovyanGFX** by lovyan03 - Graphics library
- **Adafruit** - BNO055 library and sensor support
- **Bosch Sensortec** - BNO055 sensor

## Support

For issues, questions, or suggestions:
- Open an issue on GitHub
- Check existing issues for solutions
- Provide detailed information (board type, sensor module, error messages)

## Version History

- **v1.0.0** (2025) - Initial release
  - Full compass functionality
  - Auto/manual calibration
  - NVS storage
  - Touch interface
  - ESP32/ESP32-S3 support

---

**Made with ❤️ for the maker community**
