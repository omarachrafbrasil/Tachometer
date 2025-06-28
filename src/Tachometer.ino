/**
 * File: main.cpp
 * Author: Omar Achraf / omarachraf@gmail.com  
 * Date: 2025-06-28
 * Version: 1.0
 * Description: Example usage of Tachometer class for Arduino Mega.
 *              Demonstrates configuration, initialization, and data reading.
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
    
    // Initialize tachometer system
    if (tach.Initialize()) {
        Serial.println("Tachometer initialized successfully");
    } else {
        Serial.println("ERROR: Tachometer initialization failed");
        while (1); // Halt on initialization error
    }
    
    Serial.println("Tachometer ready - starting measurements...");
}


/**
 * Arduino main loop - read and display tachometer data.
 */
void loop() {
    // Check for new measurement data
    if (tach.IsNewDataAvailable()) {
        uint32_t frequency = tach.GetCurrentFrequencyHz();
        uint32_t rpm = tach.GetCurrentRpm();
        uint32_t totalRevs = tach.GetTotalRevolutions();
        uint32_t rawPulses = tach.GetRawPulseCount();
        uint32_t pulseInterval = tach.GetPulseIntervalMicros();
        
        // Get filtered values if filtering is enabled
        uint32_t filteredFreq = tach.GetFilteredFrequencyHz();
        uint32_t filteredRpm = tach.GetFilteredRpm();
        
        // Output measurement data
        Serial.print("Raw - Freq: ");
        Serial.print(frequency);
        Serial.print(" Hz | RPM: ");
        Serial.print(rpm);
        
        if (ENABLE_FILTERING) {
            Serial.print(" | Filtered - Freq: ");
            Serial.print(filteredFreq);
            Serial.print(" Hz | RPM: ");
            Serial.print(filteredRpm);
        }
        
        Serial.print(" | Total Revs: ");
        Serial.print(totalRevs);
        Serial.print(" | Raw Pulses: ");
        Serial.print(rawPulses);
        Serial.print(" | Pulse Interval: ");
        Serial.print(pulseInterval);
        Serial.println(" us");
    }
    
    // Small delay to prevent serial buffer overflow
    delay(10);
}
