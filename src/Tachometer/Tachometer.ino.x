/**
 * File: Tachometer.ino
 * Author: Omar Achraf / omarachraf@gmail.com  
 * Date: 2025-06-28
 * Version: 1.0
 * Description: Arduino IDE sketch for Industrial Tachometer with advanced filtering
 *              and serial command interface. Demonstrates configuration, initialization,
 *              and real-time data monitoring with multiple reset options.
 */

#include "Tachometer.h"

// Configuration constants with filtering
const uint8_t  IR_SENSOR_PIN    = 2;     // Use pin 2 (INT0) for IR sensor
const uint16_t SAMPLE_PERIOD_MS = 1000;  // 1 second sample period
const uint16_t DEBOUNCE_MICROS  = 100;   // 100 microsecond debounce
const uint8_t  PULSES_PER_REV   = 1;     // 1 pulse per revolution
const uint8_t  TIMER_NUMBER     = 1;     // Use Timer1
const bool     ENABLE_FILTERING = true;  // Enable digital filtering
const uint16_t FILTER_ALPHA     = 800;   // 0.8 filter coefficient (scaled by 1000)
const uint8_t  WINDOW_SIZE      = 5;     // 5-sample moving average

// Global tachometer instance with filtering
Tachometer tach(IR_SENSOR_PIN, SAMPLE_PERIOD_MS, DEBOUNCE_MICROS, 
               PULSES_PER_REV, TIMER_NUMBER, ENABLE_FILTERING, 
               FILTER_ALPHA, WINDOW_SIZE);


/**
 * Arduino setup function - initialize system.
 */
void setup() {
    // Initialize serial communication for data output
    Serial.begin(115200);
    
    // Wait for serial connection (useful for Leonardo/Micro)
    while (!Serial && millis() < 3000) {
        delay(10);
    }
    
    Serial.println("=== Industrial Tachometer System ===");
    Serial.println("Author: Omar Achraf <omarachraf@gmail.com>");
    Serial.println("Version: 1.0");
    Serial.println();
    
    // Display configuration
    Serial.println("Configuration:");
    Serial.print("- IR Sensor Pin: "); Serial.println(IR_SENSOR_PIN);
    Serial.print("- Sample Period: "); Serial.print(SAMPLE_PERIOD_MS); Serial.println(" ms");
    Serial.print("- Debounce Time: "); Serial.print(DEBOUNCE_MICROS); Serial.println(" µs");
    Serial.print("- Pulses/Rev: "); Serial.println(PULSES_PER_REV);
    Serial.print("- Timer Number: "); Serial.println(TIMER_NUMBER);
    Serial.print("- Filtering: "); Serial.println(ENABLE_FILTERING ? "ENABLED" : "DISABLED");
    if (ENABLE_FILTERING) {
        Serial.print("- Filter Alpha: "); Serial.print(FILTER_ALPHA); Serial.println(" (0.8)");
        Serial.print("- Window Size: "); Serial.println(WINDOW_SIZE);
    }
    Serial.println();
    
    // Initialize tachometer system
    Serial.print("Initializing tachometer... ");
    if (tach.Initialize()) {
        Serial.println("SUCCESS!");
    } else {
        Serial.println("FAILED!");
        Serial.println("ERROR: Tachometer initialization failed");
        Serial.println("Check:");
        Serial.println("- IR sensor connection");
        Serial.println("- Pin number (must be 2,3,18,19,20,21)");
        Serial.println("- Timer conflicts with other libraries");
        while (1) {
            delay(1000);
            Serial.print(".");
        }
    }
    
    Serial.println();
    Serial.println("Tachometer ready - starting measurements...");
    Serial.println("Commands: 'r'=reset, 'f'=toggle filter, 'h'=help");
    Serial.println("========================================");
}


/**
 * Arduino main loop - read and display tachometer data.
 */
void loop() {
    // Check for new measurement data
    if (tach.IsNewDataAvailable()) {
        displayMeasurements();
    }
    
    // Process serial commands
    if (Serial.available()) {
        String command = Serial.readStringUntil('\n');
        processCommand(command);
    }
    
    // Small delay to prevent serial buffer overflow
    delay(10);
}


/**
 * Display measurement data in formatted output.
 */
void displayMeasurements() {
    uint32_t frequency = tach.GetCurrentFrequencyHz();
    uint32_t rpm = tach.GetCurrentRpm();
    uint32_t totalRevs = tach.GetTotalRevolutions();
    uint32_t rawPulses = tach.GetRawPulseCount();
    uint32_t pulseInterval = tach.GetPulseIntervalMicros();
    
    // Get filtered values if filtering is enabled
    uint32_t filteredFreq = tach.GetFilteredFrequencyHz();
    uint32_t filteredRpm = tach.GetFilteredRpm();
    
    // Create timestamp
    unsigned long timestamp = millis();
    
    // Output measurement data
    Serial.print("[");
    Serial.print(timestamp);
    Serial.print("ms] ");
    
    if (ENABLE_FILTERING) {
        Serial.print("Raw: ");
        Serial.print(rpm);
        Serial.print(" RPM (");
        Serial.print(frequency);
        Serial.print(" Hz) | Filtered: ");
        Serial.print(filteredRpm);
        Serial.print(" RPM (");
        Serial.print(filteredFreq);
        Serial.print(" Hz)");
    } else {
        Serial.print("RPM: ");
        Serial.print(rpm);
        Serial.print(" | Freq: ");
        Serial.print(frequency);
        Serial.print(" Hz");
    }
    
    Serial.print(" | Total: ");
    Serial.print(totalRevs);
    Serial.print(" revs | Pulses: ");
    Serial.print(rawPulses);
    
    if (pulseInterval > 0) {
        Serial.print(" | Interval: ");
        Serial.print(pulseInterval);
        Serial.print(" µs");
    }
    
    Serial.println();
}


/**
 * Process serial commands for system control.
 */
void processCommand(String cmd) {
    cmd.trim();
    cmd.toLowerCase();
    
    if (cmd == "r" || cmd == "reset") {
        tach.ResetCounters();
        Serial.println("✅ All counters reset!");
        
    } else if (cmd == "rf" || cmd == "reset filters") {
        tach.ResetFilters();
        Serial.println("✅ Filters reset!");
        
    } else if (cmd == "rr" || cmd == "reset revs") {
        tach.ResetRevolutionCounters();
        Serial.println("✅ Revolution counters reset!");
        
    } else if (cmd == "rs" || cmd == "reset system") {
        tach.ResetSystem();
        Serial.println("✅ System completely reset!");
        
    } else if (cmd == "f" || cmd == "filter") {
        // Toggle filtering
        bool currentState = (tach.GetFilteredFrequencyHz() > 0);
        tach.SetFilteringEnabled(!currentState);
        Serial.print("🔧 Filtering ");
        Serial.println(!currentState ? "ENABLED" : "DISABLED");
        
    } else if (cmd.startsWith("alpha ")) {
        // Set filter alpha: "alpha 750"
        uint16_t newAlpha = cmd.substring(6).toInt();
        if (newAlpha <= 1000) {
            tach.SetFilterParameters(newAlpha, WINDOW_SIZE);
            Serial.print("🔧 Filter alpha set to: ");
            Serial.print(newAlpha);
            Serial.print(" (");
            Serial.print(newAlpha / 10.0);
            Serial.println("%)");
        } else {
            Serial.println("❌ Invalid alpha (0-1000)");
        }
        
    } else if (cmd.startsWith("window ")) {
        // Set window size: "window 8"
        uint8_t newWindow = cmd.substring(7).toInt();
        if (newWindow >= 1 && newWindow <= 20) {
            tach.SetFilterParameters(FILTER_ALPHA, newWindow);
            Serial.print("🔧 Window size set to: ");
            Serial.println(newWindow);
        } else {
            Serial.println("❌ Invalid window size (1-20)");
        }
        
    } else if (cmd.startsWith("debounce ")) {
        // Set debounce time: "debounce 150"
        uint16_t newDebounce = cmd.substring(9).toInt();
        tach.SetDebounceTime(newDebounce);
        Serial.print("🔧 Debounce time set to: ");
        Serial.print(newDebounce);
        Serial.println(" µs");
        
    } else if (cmd.startsWith("period ")) {
        // Set sample period: "period 500"
        uint16_t newPeriod = cmd.substring(7).toInt();
        if (tach.SetSamplePeriod(newPeriod)) {
            Serial.print("🔧 Sample period set to: ");
            Serial.print(newPeriod);
            Serial.println(" ms");
        } else {
            Serial.println("❌ Invalid period (minimum 100ms)");
        }
        
    } else if (cmd == "s" || cmd == "status") {
        displaySystemStatus();
        
    } else if (cmd == "h" || cmd == "help") {
        displayHelp();
        
    } else if (cmd == "test") {
        performSystemTest();
        
    } else if (cmd.length() > 0) {
        Serial.print("❌ Unknown command: '");
        Serial.print(cmd);
        Serial.println("'. Type 'h' for help.");
    }
}


/**
 * Display complete system status.
 */
void displaySystemStatus() {
    Serial.println();
    Serial.println("📊 SYSTEM STATUS");
    Serial.println("================");
    
    // Current measurements
    Serial.println("Current Measurements:");
    Serial.print("  Raw RPM: "); Serial.println(tach.GetCurrentRpm());
    Serial.print("  Raw Frequency: "); Serial.print(tach.GetCurrentFrequencyHz()); Serial.println(" Hz");
    
    if (ENABLE_FILTERING) {
        Serial.print("  Filtered RPM: "); Serial.println(tach.GetFilteredRpm());
        Serial.print("  Filtered Freq: "); Serial.print(tach.GetFilteredFrequencyHz()); Serial.println(" Hz");
    }
    
    Serial.print("  Total Revolutions: "); Serial.println(tach.GetTotalRevolutions());
    Serial.print("  Raw Pulse Count: "); Serial.println(tach.GetRawPulseCount());
    Serial.print("  Pulse Interval: "); Serial.print(tach.GetPulseIntervalMicros()); Serial.println(" µs");
    
    // System configuration
    Serial.println();
    Serial.println("Configuration:");
    Serial.print("  IR Pin: "); Serial.println(IR_SENSOR_PIN);
    Serial.print("  Timer: "); Serial.println(TIMER_NUMBER);
    Serial.print("  Sample Period: "); Serial.print(SAMPLE_PERIOD_MS); Serial.println(" ms");
    Serial.print("  Debounce: "); Serial.print(DEBOUNCE_MICROS); Serial.println(" µs");
    Serial.print("  Pulses/Rev: "); Serial.println(PULSES_PER_REV);
    Serial.print("  Filtering: "); Serial.println(ENABLE_FILTERING ? "ENABLED" : "DISABLED");
    
    if (ENABLE_FILTERING) {
        Serial.print("  Filter Alpha: "); Serial.print(FILTER_ALPHA); Serial.println(" (0-1000)");
        Serial.print("  Window Size: "); Serial.println(WINDOW_SIZE);
    }
    
    // System health
    Serial.println();
    Serial.println("System Health:");
    
    int freeMem = freeMemory();
    if (freeMem >= 0) {
        Serial.print("  Free RAM: "); 
        Serial.print(freeMem); 
        Serial.println(" bytes");
    } else {
        Serial.println("  Free RAM: Not available on this platform");
    }
    
    Serial.print("  Uptime: "); 
    Serial.print(millis() / 1000); 
    Serial.println(" seconds");
    
    Serial.println("================");
    Serial.println();
}


/**
 * Display help information.
 */
void displayHelp() {
    Serial.println();
    Serial.println("📖 AVAILABLE COMMANDS");
    Serial.println("=====================");
    Serial.println("Reset Commands:");
    Serial.println("  r, reset           - Reset all counters and filters");
    Serial.println("  rf, reset filters  - Reset only filters (preserve counters)");
    Serial.println("  rr, reset revs     - Reset only revolution counters");
    Serial.println("  rs, reset system   - Complete system reset");
    Serial.println();
    Serial.println("Filter Commands:");
    Serial.println("  f, filter          - Toggle filtering on/off");
    Serial.println("  alpha <0-1000>     - Set filter coefficient (800 = 0.8)");
    Serial.println("  window <1-20>      - Set moving average window size");
    Serial.println();
    Serial.println("Configuration Commands:");
    Serial.println("  debounce <µs>      - Set debounce time in microseconds");
    Serial.println("  period <ms>        - Set sample period (min 100ms)");
    Serial.println();
    Serial.println("Information Commands:");
    Serial.println("  s, status          - Show complete system status");
    Serial.println("  h, help            - Show this help");
    Serial.println("  test               - Perform system diagnostics");
    Serial.println("=====================");
    Serial.println();
}


/**
 * Perform system diagnostics test.
 */
void performSystemTest() {
    Serial.println();
    Serial.println("🔧 SYSTEM DIAGNOSTICS");
    Serial.println("====================");
    
    // Test 1: Memory check
    Serial.print("Memory Test: ");
    int freeMem = freeMemory();
    
    if (freeMem < 0) {
        Serial.println("⚠️  SKIPPED (architecture not supported)");
    } else if (freeMem > 1000) {
        Serial.print("✅ PASS (");
        Serial.print(freeMem);
        Serial.println(" bytes free)");
    } else {
        Serial.print("⚠️  WARNING (");
        Serial.print(freeMem);
        Serial.println(" bytes free - low memory!)");
    }
    
    // Test 2: Pulse detection test
    Serial.println();
    Serial.println("Pulse Detection Test:");
    Serial.println("  Monitoring for 5 seconds...");
    
    uint32_t startCount = tach.GetRawPulseCount();
    uint32_t startTime = millis();
    
    while (millis() - startTime < 5000) {
        if (tach.IsNewDataAvailable()) {
            uint32_t currentCount = tach.GetRawPulseCount();
            if (currentCount > 0) {
                Serial.print("  ✅ Pulse detected! Count: ");
                Serial.println(currentCount);
            }
        }
        delay(100);
    }
    
    uint32_t endCount = tach.GetRawPulseCount();
    uint32_t totalPulses = endCount - startCount;
    
    Serial.print("  Result: ");
    if (totalPulses > 0) {
        Serial.print("✅ ");
        Serial.print(totalPulses);
        Serial.println(" pulses detected");
    } else {
        Serial.println("⚠️  No pulses detected");
        Serial.println("  Check:");
        Serial.println("    - IR sensor connection");
        Serial.println("    - Sensor power supply");
        Serial.println("    - Object passing through sensor");
    }
    
    // Test 3: Timer test
    Serial.println();
    Serial.print("Timer Test: ");
    unsigned long testStart = millis();
    delay(1000);
    unsigned long testEnd = millis();
    unsigned long elapsed = testEnd - testStart;
    
    if (elapsed >= 990 && elapsed <= 1010) {
        Serial.println("✅ PASS (timing accurate)");
    } else {
        Serial.print("⚠️  WARNING (expected ~1000ms, got ");
        Serial.print(elapsed);
        Serial.println("ms)");
    }
    
    Serial.println("====================");
    Serial.println("✅ Diagnostics complete!");
    Serial.println();
}


/**
 * Calculate free memory (for system monitoring).
 */
int freeMemory() {
#ifdef __arm__
    // ARM processors (Due, Zero, etc.)
    char top;
    return &top - reinterpret_cast<char*>(sbrk(0));
#elif defined(ARDUINO_ARCH_ESP32)
    // ESP32
    return ESP.getFreeHeap();
#elif defined(ARDUINO_ARCH_ESP8266)
    // ESP8266
    return ESP.getFreeHeap();
#elif defined(__AVR__)
    // AVR processors (Uno, Mega, etc.)
    extern int __heap_start, *__brkval;
    int v;
    return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
#else
    // Fallback for other architectures
    return -1; // Unknown architecture
#endif
}