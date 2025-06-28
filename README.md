# Industrial Tachometer Library

A high-precision, industrial-grade tachometer library for Arduino Mega, ESP32, and Teensy 4.1 platforms, designed following DO-178C standards for critical applications.

## 🚀 Key Features

- **Dual-Interrupt Architecture**: External pin interrupt for pulse counting + timer interrupt for precise time base
- **Integer-Only Operations**: Optimized for maximum performance without floating-point overhead
- **Configurable Digital Filtering**: Low-pass filter + moving average with runtime parameter adjustment
- **Thread-Safe Design**: Atomic variable access and ISR-safe operations
- **Multi-Platform Support**: Arduino IDE and PlatformIO compatible
- **Multiple Reset Options**: Granular control over counters, filters, and system state

## 📊 Performance Specifications

| Platform | Max Frequency | ISR Latency | Timer Resolution | Memory Usage |
|----------|---------------|-------------|------------------|--------------|
| **Arduino Mega** | 50 kHz | ~4 µs | 64 µs | ~200 bytes |
| **ESP32** | 200 kHz | ~1 µs | 1 µs | ~300 bytes |
| **Teensy 4.1** | 500 kHz | ~0.5 µs | 0.1 µs | ~400 bytes |

## 🛠️ Quick Start

```cpp
#include "Tachometer.h"

// Configure tachometer: pin 2, 1s period, 100µs debounce, 4 pulses/rev, Timer1, filtering enabled
Tachometer tach(2, 1000, 100, 4, 1, true, 800, 5);

void setup() {
    Serial.begin(115200);
    
    if (!tach.Initialize()) {
        Serial.println("Tachometer initialization failed!");
        while(1);
    }
    
    Serial.println("Tachometer ready!");
}

void loop() {
    if (tach.IsNewDataAvailable()) {
        Serial.print("RPM: ");
        Serial.print(tach.GetCurrentRpm());
        Serial.print(" | Filtered RPM: ");
        Serial.print(tach.GetFilteredRpm());
        Serial.print(" | Frequency: ");
        Serial.print(tach.GetCurrentFrequencyHz());
        Serial.println(" Hz");
    }
    delay(10);
}
```

## 📈 Applications

- **Industrial Motor Control**: High-precision RPM monitoring for critical machinery
- **Wind Turbine Monitoring**: Stable measurements in noisy electromagnetic environments  
- **Test Bench Equipment**: Laboratory-grade accuracy with configurable filtering
- **Multi-Axis Systems**: Independent measurement of multiple rotating components
- **Quality Control**: Production line speed verification and statistics

## 🔧 Constructor Parameters

```cpp
Tachometer(uint8_t irPin,                    // IR sensor input pin (2,3,18,19,20,21)
           uint16_t samplePeriodMs = 1000,   // Timer interrupt period (100-65535ms)
           uint16_t debounceMicros = 100,    // Noise filter time (0-65535µs)
           uint8_t pulsesPerRev = 1,         // Encoder pulses per revolution
           uint8_t timerNum = 1,             // Timer number (1,3,4,5)
           bool enableFiltering = false,     // Enable digital filtering
           uint16_t filterAlpha = 800,       // Low-pass coefficient (0-1000)
           uint8_t windowSize = 5);          // Moving average window (1-20)
```

## 🎛️ Reset Operations

- **`ResetCounters()`**: Complete reset of all counters and filters
- **`ResetFilters()`**: Reset only digital filters, preserve revolution counters
- **`ResetRevolutionCounters()`**: Reset only pulse/revolution counts, preserve filter state
- **`ResetSystem()`**: Full system reset including timing references

## 📚 Documentation

For complete technical documentation, hardware connection diagrams, filtering theory, troubleshooting guide, and advanced examples, see:

**📖 [Complete Technical Documentation](Tachometer.md)**

## 🔌 Hardware Requirements

- **IR Sensor**: Phototransistor, photodiode, or optocoupler
- **Pull-up Resistor**: 10kΩ recommended
- **Noise Filtering**: 100nF capacitor + RC filter for industrial environments
- **Shielded Cable**: Twisted pair or coaxial for high-noise applications

## 🏗️ Installation

### PlatformIO (Recommended)
```ini
[env:megaatmega2560]
platform = atmelavr
board = megaatmega2560
framework = arduino
lib_deps = 
    https://github.com/yourusername/industrial-tachometer.git
```

### Arduino IDE
1. Download library files
2. Copy to `Arduino/libraries/IndustrialTachometer/`
3. Include in sketch: `#include "Tachometer.h"`

## 👨‍💻 Author

**Omar Achraf** <omarachraf@gmail.com>

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 🤝 Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## ⭐ Support

If this library helped your project, please consider giving it a star! ⭐

For technical support, bug reports, or feature requests, please open an issue on GitHub.

---

**Note**: This library follows DO-178C software development standards and is suitable for safety-critical applications with proper verification and validation procedures.