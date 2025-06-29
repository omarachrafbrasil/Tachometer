/**
 * File: Tachometer.h
 * Author: Omar Achraf / omarachraf@gmail.com
 * Date: 2025-06-28
 * Version: 1.0
 * Description: High-precision tachometer system for Arduino Mega/ESP32/Teensy using dual 
 *              interrupt architecture - external pin interrupt for pulse counting and timer 
 *              interrupt for time base. Implements noise filtering and atomic variable access
 *              for industrial-grade frequency measurement without floating point operations.
 *              Responsible for IR sensor pulse counting and timing control optimization.
 */

#ifndef TACHOMETER_H
#define TACHOMETER_H

#include <Arduino.h>

// Platform-specific includes
#ifdef ARDUINO_ARCH_AVR
    // Arduino Mega, Uno, etc. - AVR timers
    #include <avr/interrupt.h>
#elif defined(ESP32)
    // ESP32 - Use hardware timers
    #include "driver/timer.h"
    #include "esp_timer.h"
#elif defined(ARDUINO_ARCH_ESP32)
    // ESP32 Arduino framework
    #include "driver/timer.h"
    #include "esp_timer.h"
#elif defined(__arm__) && defined(TEENSYDUINO)
    // Teensy 4.x - ARM Cortex-M7
    #include "IntervalTimer.h"
#endif

#ifdef _WIN32
    #include <windows.h>
    #include <stdio.h>
#endif


/**
 * Class responsible for high-precision frequency measurement using dual interrupt system.
 * Implements hardware-level pulse counting with software noise filtering and
 * atomic variable operations for thread-safe data access between ISR contexts.
 * Optimized for integer-only operations to maximize performance on AVR/ARM platforms.
 */
class Tachometer {
private:
    // Hardware configuration constants
    static const uint16_t DEFAULT_DEBOUNCE_MICROS = 100;    // Default minimum pulse width
    static const uint16_t DEFAULT_SAMPLE_PERIOD_MS = 1000;  // Default timer period (1 second)
    static const uint8_t DEFAULT_PULSES_PER_REV = 1;        // Default encoder resolution
    
    // Instance configuration variables
    uint8_t _irSensorPin;                    // IR sensor input pin number
    uint16_t _debounceMicros;               // Minimum pulse width filter
    uint16_t _samplePeriodMs;               // Timer interrupt period in milliseconds
    uint8_t _pulsesPerRevolution;           // Encoder pulses per shaft revolution
    
    // Digital filtering configuration
    bool _filteringEnabled;                 // Enable/disable digital filtering
    uint16_t _filterAlpha;                  // Low-pass filter coefficient (0-1000)
    uint8_t _windowSize;                    // Moving average window size
    
    // Filtering variables
    uint32_t _filteredFrequency;            // Low-pass filtered frequency
    uint32_t _filteredRpm;                  // Low-pass filtered RPM
    uint32_t _frequencyHistory[20];         // Circular buffer for moving average
    uint8_t _historyIndex;                  // Current position in history buffer
    uint8_t _historyCount;                  // Number of valid entries in history
    
    // Volatile variables for ISR communication (thread-safe atomic access required)
    volatile uint32_t _pulseCounter;         // Raw pulse count from sensor ISR
    volatile uint32_t _pulsesPerSample;      // Pulses captured in last sample period
    volatile uint32_t _lastInterruptMicros;  // Last valid interrupt timestamp
    volatile uint32_t _previousInterruptMicros; // Previous interrupt timestamp for interval calculation
    volatile uint32_t _pulseIntervalMicros;  // Time interval between last two pulses
    volatile bool _newDataAvailable;         // Flag indicating fresh frequency data
    
    // Non-volatile operational variables
    uint32_t _currentFrequencyHz;           // Current frequency in Hz (integer)
    uint32_t _currentRpm;                   // Current RPM (integer)
    uint32_t _totalRevolutions;             // Lifetime revolution counter
    bool _systemInitialized;               // System initialization status
    
    // Timer configuration
    uint8_t _timerNumber;                   // Timer number to use (1, 3, 4, or 5)
    
    // Platform-specific timer handles
#ifdef ESP32
    esp_timer_handle_t _espTimerHandle;     // ESP32 timer handle
#elif defined(ARDUINO_ARCH_ESP32)
    esp_timer_handle_t _espTimerHandle;     // ESP32 timer handle
#elif defined(__arm__) && defined(TEENSYDUINO)
    IntervalTimer _teensyTimer;             // Teensy interval timer
#endif


public:
    /**
     * Constructor for tachometer with configurable parameters including digital filtering.
     * Allows customization of all operational parameters for different sensor types.
     * Args:
     *     irPin - Digital pin number for IR sensor input (must support external interrupt)
     *     samplePeriodMs - Timer interrupt period in milliseconds (default: 1000ms)
     *     debounceMicros - Minimum pulse width for noise filtering (default: 100us)
     *     pulsesPerRev - Number of pulses per complete revolution (default: 1)
     *     timerNum - Timer number to use: 1, 3, 4, or 5 (default: 1)
     *     enableFiltering - Enable digital low-pass filtering (default: false)
     *     filterAlpha - Low-pass filter coefficient 0.0-1.0, higher = less filtering (default: 0.8)
     *     windowSize - Moving average window size for additional smoothing (default: 5)
     * Returns:
     *     None (constructor)
     */
    Tachometer(uint8_t irPin, 
              uint16_t samplePeriodMs = DEFAULT_SAMPLE_PERIOD_MS,
              uint16_t debounceMicros = DEFAULT_DEBOUNCE_MICROS,
              uint8_t pulsesPerRev = DEFAULT_PULSES_PER_REV,
              uint8_t timerNum = 1,
              bool enableFiltering = false,
              uint16_t filterAlpha = 800,  // 0.8 * 1000 for integer math
              uint8_t windowSize = 5);


    /**
     * Initialize tachometer system with hardware configuration and interrupt setup.
     * Configures specified timer for precise intervals and external interrupt for pulse detection.
     * Args:
     *     None
     * Returns:
     *     bool - True if initialization successful, false otherwise
     */
    bool Initialize();


    /**
     * Get current frequency reading in Hz as integer value.
     * Ensures thread-safe reading of frequency data calculated by timer interrupt.
     * Args:
     *     None
     * Returns:
     *     uint32_t - Current frequency in Hz (pulses per second)
     */
    uint32_t GetCurrentFrequencyHz();


    /**
     * Get current RPM reading as integer value.
     * Calculates RPM based on frequency and pulses per revolution configuration.
     * Args:
     *     None
     * Returns:
     *     uint32_t - Current RPM value (revolutions per minute)
     */
    uint32_t GetCurrentRpm();


    /**
     * Get total revolution count since system startup.
     * Provides cumulative revolution counting for maintenance tracking.
     * Args:
     *     None
     * Returns:
     *     uint32_t - Total number of complete revolutions
     */
    uint32_t GetTotalRevolutions();


    /**
     * Check if new frequency data is available for reading.
     * Used to optimize data processing rate and detect fresh measurements.
     * Args:
     *     None
     * Returns:
     *     bool - True if new data ready for reading, false otherwise
     */
    bool IsNewDataAvailable();


    /**
     * Reset all counters and statistics to initial state.
     * Provides system reset capability without hardware restart.
     * Args:
     *     None
     * Returns:
     *     void
     */
    void ResetCounters();


    /**
     * Reset only filter state and history buffer.
     * Clears filtering memory while preserving revolution counters.
     * Args:
     *     None
     * Returns:
     *     void
     */
    void ResetFilters();


    /**
     * Reset only revolution and pulse counters.
     * Preserves filter state and configuration.
     * Args:
     *     None
     * Returns:
     *     void
     */
    void ResetRevolutionCounters();


    /**
     * Perform complete system reset to factory defaults.
     * Resets all counters, filters, and restores initial configuration.
     * Args:
     *     None
     * Returns:
     *     void
     */
    void ResetSystem();


    /**
     * Get raw pulse count from last sample period.
     * Provides access to unprocessed pulse data for advanced analysis.
     * Args:
     *     None
     * Returns:
     *     uint32_t - Raw pulse count from last sampling interval
     */
    uint32_t GetRawPulseCount();


    /**
     * Get time interval between last two pulses in microseconds.
     * Provides pulse-to-pulse timing information for instantaneous speed calculation.
     * Args:
     *     None
     * Returns:
     *     uint32_t - Time interval in microseconds between last two valid pulses (0 if no pulses detected)
     */
    uint32_t GetPulseIntervalMicros();


    /**
     * Change sample period during runtime.
     * Allows dynamic adjustment of measurement time base for different applications.
     * Args:
     *     newPeriodMs - New sample period in milliseconds (minimum: 100ms)
     * Returns:
     *     bool - True if change successful, false if invalid parameter
     */
    bool SetSamplePeriod(uint16_t newPeriodMs);


    /**
     * Change debounce filter time during runtime.
     * Allows dynamic adjustment of noise filtering for different sensor characteristics.
     * Args:
     *     newDebounceMicros - New debounce time in microseconds
     * Returns:
     *     void
     */
    void SetDebounceTime(uint16_t newDebounceMicros);


    /**
     * Enable or disable digital filtering during runtime.
     * Allows dynamic control of signal filtering for different operating conditions.
     * Args:
     *     enabled - True to enable filtering, false for raw measurements
     * Returns:
     *     void
     */
    void SetFilteringEnabled(bool enabled);


    /**
     * Configure digital filter parameters during runtime.
     * Allows adjustment of filtering characteristics for optimal performance.
     * Args:
     *     filterAlpha - Low-pass filter coefficient (0-1000, where 1000 = no filtering)
     *     windowSize - Moving average window size (1-20)
     * Returns:
     *     bool - True if parameters valid and applied, false otherwise
     */
    bool SetFilterParameters(uint16_t filterAlpha, uint8_t windowSize);


    /**
     * Get filtered frequency reading in Hz as integer value.
     * Returns digitally filtered frequency for smoother measurements.
     * Args:
     *     None
     * Returns:
     *     uint32_t - Filtered frequency in Hz (0 if filtering disabled)
     */
    uint32_t GetFilteredFrequencyHz();


    /**
     * Get filtered RPM reading as integer value.
     * Returns digitally filtered RPM for smoother measurements.
     * Args:
     *     None
     * Returns:
     *     uint32_t - Filtered RPM value (0 if filtering disabled)
     */
    uint32_t GetFilteredRpm();


    /**
     * Pulse detection interrupt service routine.
     * Called by external interrupt when IR sensor detects pulse edge.
     * Implements debounce filtering and pulse counting.
     * Args:
     *     None
     * Returns:
     *     void
     */
    void HandlePulseInterrupt();


    /**
     * Timer interrupt service routine.
     * Called by timer interrupt at configured sample rate.
     * Calculates frequency and RPM from accumulated pulse count.
     * Args:
     *     None
     * Returns:
     *     void
     */
    void HandleTimerInterrupt();


    /**
     * Get timer number for ISR access.
     * Provides read-only access to timer configuration for interrupt handlers.
     * Args:
     *     None
     * Returns:
     *     uint8_t - Current timer number (1, 3, 4, or 5)
     */
    uint8_t GetTimerNumber() const;


private:
    /**
     * Configure specified timer for precise interrupt generation.
     * Sets up CTC mode with appropriate prescaler for desired interrupt rate.
     * Args:
     *     None
     * Returns:
     *     bool - True if timer configuration successful, false otherwise
     */
    bool ConfigureTimer();


    /**
     * Configure external interrupt for IR sensor pulse detection.
     * Sets up interrupt with rising edge detection on specified pin.
     * Args:
     *     None
     * Returns:
     *     bool - True if interrupt configuration successful, false otherwise
     */
    bool ConfigureExternalInterrupt();


    /**
     * Perform atomic read of 32-bit volatile variable.
     * Ensures thread-safe access to multi-byte variables shared between ISR and main code.
     * Args:
     *     volatileVar - Reference to volatile 32-bit variable
     * Returns:
     *     uint32_t - Atomic copy of variable value
     */
    uint32_t AtomicRead32(volatile uint32_t& volatileVar);


    /**
     * Perform atomic write to 32-bit volatile variable.
     * Ensures thread-safe modification of multi-byte variables shared between ISR and main code.
     * Args:
     *     volatileVar - Reference to volatile 32-bit variable
     *     value - New value to write atomically
     * Returns:
     *     void
     */
    void AtomicWrite32(volatile uint32_t& volatileVar, uint32_t value);


    /**
     * Calculate timer compare value for desired interrupt frequency.
     * Computes optimal prescaler and compare values for specified timer period.
     * Args:
     *     periodMs - Desired timer period in milliseconds
     * Returns:
     *     uint16_t - Timer compare register value
     */
    uint16_t CalculateTimerCompareValue(uint16_t periodMs);


    /**
     * Apply digital filtering to measurements.
     * Implements low-pass filter and moving average for noise reduction.
     * Args:
     *     None
     * Returns:
     *     void
     */
    void ApplyDigitalFilter();

}; // end class Tachometer

#endif // TACHOMETER_H