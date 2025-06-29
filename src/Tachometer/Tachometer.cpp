/**
 * File: Tachometer.cpp
 * Author: Omar Achraf / omarachraf@gmail.com
 * Date: 2025-06-28
 * Version: 1.0
 * Description: Implementation of high-precision tachometer system with dual interrupt
 *              architecture, noise filtering, and atomic variable operations.
 *              Optimized for integer-only operations on Arduino Mega/ESP32/Teensy platforms.
 */

#include "Tachometer.h"


// Static instance pointer for ISR callbacks
static Tachometer* activeInstance = nullptr;

// ESP32 critical section mutex
#if defined(ESP32) || defined(ARDUINO_ARCH_ESP32)
static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
#endif


/**
 * Constructor initializes tachometer configuration parameters.
 */
Tachometer::Tachometer(uint8_t irPin, 
                      uint16_t samplePeriodMs,
                      uint16_t debounceMicros,
                      uint8_t pulsesPerRev,
                      uint8_t timerNum,
                      bool enableFiltering,
                      uint16_t filterAlpha,
                      uint8_t windowSize) :
    _irSensorPin(irPin),
    _samplePeriodMs(samplePeriodMs),
    _debounceMicros(debounceMicros),
    _pulsesPerRevolution(pulsesPerRev),
    _timerNumber(timerNum),
    _filteringEnabled(enableFiltering),
    _filterAlpha(filterAlpha),
    _windowSize(windowSize),
    _filteredFrequency(0),
    _filteredRpm(0),
    _historyIndex(0),
    _historyCount(0),
    _pulseCounter(0),
    _pulsesPerSample(0),
    _lastInterruptMicros(0),
    _previousInterruptMicros(0),
    _pulseIntervalMicros(0),
    _newDataAvailable(false),
    _currentFrequencyHz(0),
    _currentRpm(0),
    _totalRevolutions(0),
    _systemInitialized(false) {
    
    // Validate timer number for Arduino Mega
    if (_timerNumber != 1 && _timerNumber != 3 && _timerNumber != 4 && _timerNumber != 5) {
        _timerNumber = 1; // Default to Timer1 if invalid
    }
    
    // Validate sample period (minimum 100ms)
    if (_samplePeriodMs < 100) {
        _samplePeriodMs = 100;
    }
    
    // Validate filter parameters
    if (_filterAlpha > 1000) {
        _filterAlpha = 1000; // Maximum value for no filtering
    }
    if (_windowSize < 1 || _windowSize > 20) {
        _windowSize = 5; // Default safe value
    }
    
    // Initialize history buffer
    for (uint8_t i = 0; i < 20; i++) {
        _frequencyHistory[i] = 0;
    }
    
    // Initialize platform-specific timer handles
#if defined(ESP32) || defined(ARDUINO_ARCH_ESP32)
    _espTimerHandle = nullptr;
#endif
}


/**
 * Initialize hardware and interrupt configuration.
 */
bool Tachometer::Initialize() {
    if (_systemInitialized) {
        return true;
    }
    
    // Set static instance pointer for ISR callbacks
    activeInstance = this;
    
    // Configure IR sensor pin as input with pullup
    pinMode(_irSensorPin, INPUT_PULLUP);
    
    // Initialize timing variables
    _lastInterruptMicros = micros();
    
    // Configure timer interrupt
    if (!ConfigureTimer()) {
        return false;
    }
    
    // Configure external interrupt
    if (!ConfigureExternalInterrupt()) {
        return false;
    }
    
    _systemInitialized = true;
    return true;
}


/**
 * Get current frequency in Hz with atomic access protection.
 */
uint32_t Tachometer::GetCurrentFrequencyHz() {
    return AtomicRead32(reinterpret_cast<volatile uint32_t&>(_currentFrequencyHz));
}


/**
 * Get current RPM with atomic access protection.
 */
uint32_t Tachometer::GetCurrentRpm() {
    return AtomicRead32(reinterpret_cast<volatile uint32_t&>(_currentRpm));
}


/**
 * Get total revolution count.
 */
uint32_t Tachometer::GetTotalRevolutions() {
    return _totalRevolutions;
}


/**
 * Check for new data availability.
 */
bool Tachometer::IsNewDataAvailable() {
    if (_newDataAvailable) {
        _newDataAvailable = false;
        return true;
    }
    return false;
}


/**
 * Reset all counters and statistics to initial state.
 */
void Tachometer::ResetCounters() {
    noInterrupts();
    _pulseCounter = 0;
    _pulsesPerSample = 0;
    _lastInterruptMicros = 0;
    _previousInterruptMicros = 0;
    _pulseIntervalMicros = 0;
    _totalRevolutions = 0;
    _currentFrequencyHz = 0;
    _currentRpm = 0;
    _newDataAvailable = false;
    
    // Reset filter state
    _filteredFrequency = 0;
    _filteredRpm = 0;
    _historyIndex = 0;
    _historyCount = 0;
    
    // Clear history buffer
    for (uint8_t i = 0; i < 20; i++) {
        _frequencyHistory[i] = 0;
    }
    
    interrupts();
}


/**
 * Reset only filter state and history buffer.
 */
void Tachometer::ResetFilters() {
    noInterrupts();
    
    // Reset filtered values to current raw values
    _filteredFrequency = _currentFrequencyHz;
    _filteredRpm = _currentRpm;
    
    // Reset history buffer
    _historyIndex = 0;
    _historyCount = 0;
    
    // Clear history buffer
    for (uint8_t i = 0; i < 20; i++) {
        _frequencyHistory[i] = 0;
    }
    
    interrupts();
}


/**
 * Reset only revolution and pulse counters.
 */
void Tachometer::ResetRevolutionCounters() {
    noInterrupts();
    _pulseCounter = 0;
    _pulsesPerSample = 0;
    _totalRevolutions = 0;
    _newDataAvailable = false;
    interrupts();
}


/**
 * Perform complete system reset to factory defaults.
 */
void Tachometer::ResetSystem() {
    noInterrupts();
    
    // Reset all counters
    _pulseCounter = 0;
    _pulsesPerSample = 0;
    _lastInterruptMicros = 0;
    _previousInterruptMicros = 0;
    _pulseIntervalMicros = 0;
    _totalRevolutions = 0;
    _currentFrequencyHz = 0;
    _currentRpm = 0;
    _newDataAvailable = false;
    
    // Reset filter state
    _filteredFrequency = 0;
    _filteredRpm = 0;
    _historyIndex = 0;
    _historyCount = 0;
    
    // Clear history buffer
    for (uint8_t i = 0; i < 20; i++) {
        _frequencyHistory[i] = 0;
    }
    
    // Reset timing reference
    _lastInterruptMicros = micros();
    
    interrupts();
}


/**
 * Get raw pulse count from last sample.
 */
uint32_t Tachometer::GetRawPulseCount() {
    return AtomicRead32(_pulsesPerSample);
}


/**
 * Get time interval between last two pulses in microseconds.
 */
uint32_t Tachometer::GetPulseIntervalMicros() {
    return AtomicRead32(_pulseIntervalMicros);
}


/**
 * Change sample period during runtime.
 */
bool Tachometer::SetSamplePeriod(uint16_t newPeriodMs) {
    if (newPeriodMs < 100) {
        return false; // Minimum period validation
    }
    
    noInterrupts();
    _samplePeriodMs = newPeriodMs;
    
    // Recalculate timer settings based on platform
#ifdef ARDUINO_ARCH_AVR
    // Arduino Mega - Update timer compare register
    uint16_t compareValue = CalculateTimerCompareValue(_samplePeriodMs);
    
    switch (_timerNumber) {
        case 1: OCR1A = compareValue; break;
        case 3: OCR3A = compareValue; break;
        case 4: OCR4A = compareValue; break;
        case 5: OCR5A = compareValue; break;
    }
#elif defined(ESP32) || defined(ARDUINO_ARCH_ESP32)
    // ESP32 - Restart timer with new period
    if (_espTimerHandle != nullptr) {
        esp_timer_stop(_espTimerHandle);
        esp_timer_start_periodic(_espTimerHandle, _samplePeriodMs * 1000); // Convert to microseconds
    }
#elif defined(__arm__) && defined(TEENSYDUINO)
    // Teensy - Update interval timer
    _teensyTimer.update(_samplePeriodMs * 1000); // Convert to microseconds
#endif
    
    interrupts();
    return true;
}


/**
 * Change debounce time during runtime.
 */
void Tachometer::SetDebounceTime(uint16_t newDebounceMicros) {
    _debounceMicros = newDebounceMicros;
}


/**
 * Enable or disable digital filtering.
 */
void Tachometer::SetFilteringEnabled(bool enabled) {
    _filteringEnabled = enabled;
    if (!enabled) {
        // Reset filter state when disabling
        _filteredFrequency = _currentFrequencyHz;
        _filteredRpm = _currentRpm;
        _historyCount = 0;
        _historyIndex = 0;
    }
}


/**
 * Configure filter parameters during runtime.
 */
bool Tachometer::SetFilterParameters(uint16_t filterAlpha, uint8_t windowSize) {
    if (filterAlpha > 1000 || windowSize < 1 || windowSize > 20) {
        return false;
    }
    
    _filterAlpha = filterAlpha;
    _windowSize = windowSize;
    
    // Reset filter state with new parameters
    _historyCount = 0;
    _historyIndex = 0;
    
    return true;
}


/**
 * Get filtered frequency reading.
 */
uint32_t Tachometer::GetFilteredFrequencyHz() {
    if (!_filteringEnabled) {
        return 0; // Return 0 when filtering disabled
    }
    return _filteredFrequency;
}


/**
 * Get filtered RPM reading.
 */
uint32_t Tachometer::GetFilteredRpm() {
    if (!_filteringEnabled) {
        return 0; // Return 0 when filtering disabled
    }
    return _filteredRpm;
}


/**
 * Pulse detection interrupt handler with debounce filtering.
 */
void Tachometer::HandlePulseInterrupt() {
    uint32_t currentMicros = micros();
    
    // Debounce filter - ignore pulses too close together
    if (currentMicros - _lastInterruptMicros >= _debounceMicros) {
        // Calculate interval between pulses
        if (_lastInterruptMicros > 0) { // Skip first pulse (no previous reference)
            _pulseIntervalMicros = currentMicros - _lastInterruptMicros;
        }
        
        // Update timing references
        _previousInterruptMicros = _lastInterruptMicros;
        _lastInterruptMicros = currentMicros;
        
        // Increment pulse counter
        _pulseCounter++;
    }
}


/**
 * Timer interrupt handler for frequency calculation with optional filtering.
 */
void Tachometer::HandleTimerInterrupt() {
    // Atomic read and reset of pulse counter
    uint32_t currentPulses = _pulseCounter;
    _pulseCounter = 0;
    
    // Store pulse count for this sample period
    _pulsesPerSample = currentPulses;
    
    // Calculate frequency in Hz (pulses per second)
    // Formula: frequency = (pulses * 1000) / samplePeriodMs
    _currentFrequencyHz = (currentPulses * 1000UL) / _samplePeriodMs;
    
    // Calculate RPM based on frequency and pulses per revolution
    // Formula: RPM = (frequency * 60) / pulsesPerRevolution
    if (_pulsesPerRevolution > 0) {
        _currentRpm = (_currentFrequencyHz * 60UL) / _pulsesPerRevolution;
    }
    
    // Apply digital filtering if enabled
    if (_filteringEnabled) {
        ApplyDigitalFilter();
    }
    
    // Update total revolution counter
    if (_pulsesPerRevolution > 0) {
        _totalRevolutions += currentPulses / _pulsesPerRevolution;
    }
    
    // Signal new data availability
    _newDataAvailable = true;
}


/**
 * Get timer number for ISR access.
 */
uint8_t Tachometer::GetTimerNumber() const {
    return _timerNumber;
}


/**
 * Configure timer for precise interrupt generation.
 */
bool Tachometer::ConfigureTimer() {
#ifdef ARDUINO_ARCH_AVR
    // Arduino Mega - Configure hardware timers
    uint16_t compareValue = CalculateTimerCompareValue(_samplePeriodMs);
    
    switch (_timerNumber) {
        case 1:
            // Timer1 configuration - CTC mode with prescaler 1024
            TCCR1A = 0;
            TCCR1B = (1 << WGM12) | (1 << CS12) | (1 << CS10); // CTC mode, prescaler 1024
            OCR1A = compareValue;
            TIMSK1 |= (1 << OCIE1A); // Enable compare match interrupt
            break;
            
        case 3:
            // Timer3 configuration - CTC mode with prescaler 1024  
            TCCR3A = 0;
            TCCR3B = (1 << WGM32) | (1 << CS32) | (1 << CS30);
            OCR3A = compareValue;
            TIMSK3 |= (1 << OCIE3A);
            break;
            
        case 4:
            // Timer4 configuration - CTC mode with prescaler 1024
            TCCR4A = 0;
            TCCR4B = (1 << WGM42) | (1 << CS42) | (1 << CS40);
            OCR4A = compareValue;
            TIMSK4 |= (1 << OCIE4A);
            break;
            
        case 5:
            // Timer5 configuration - CTC mode with prescaler 1024
            TCCR5A = 0;
            TCCR5B = (1 << WGM52) | (1 << CS52) | (1 << CS50);
            OCR5A = compareValue;
            TIMSK5 |= (1 << OCIE5A);
            break;
            
        default:
            return false;
    }
    
#elif defined(ESP32) || defined(ARDUINO_ARCH_ESP32)
    // ESP32 - Use esp_timer for high precision
    esp_timer_create_args_t timerConfig = {
        .callback = [](void* arg) {
            Tachometer* instance = static_cast<Tachometer*>(arg);
            if (instance != nullptr) {
                instance->HandleTimerInterrupt();
            }
        },
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "tachometer_timer"
    };
    
    esp_err_t result = esp_timer_create(&timerConfig, &_espTimerHandle);
    if (result != ESP_OK) {
        return false;
    }
    
    // Start periodic timer
    result = esp_timer_start_periodic(_espTimerHandle, _samplePeriodMs * 1000); // Convert to microseconds
    if (result != ESP_OK) {
        esp_timer_delete(_espTimerHandle);
        _espTimerHandle = nullptr;
        return false;
    }
    
#elif defined(__arm__) && defined(TEENSYDUINO)
    // Teensy 4.x - Use IntervalTimer
    bool result = _teensyTimer.begin([this]() {
        this->HandleTimerInterrupt();
    }, _samplePeriodMs * 1000); // Convert to microseconds
    
    if (!result) {
        return false;
    }
    
    // Set high priority for timer interrupt
    _teensyTimer.priority(0);
    
#else
    // Unsupported platform
    return false;
#endif
    
    return true;
}


/**
 * Configure external interrupt for pulse detection.
 */
bool Tachometer::ConfigureExternalInterrupt() {
#ifdef ARDUINO_ARCH_AVR
    // Arduino Mega specific pin mapping
    uint8_t interruptNum;
    
    switch (_irSensorPin) {
        case 2:  interruptNum = 0; break; // INT0
        case 3:  interruptNum = 1; break; // INT1
        case 18: interruptNum = 5; break; // INT5
        case 19: interruptNum = 4; break; // INT4
        case 20: interruptNum = 3; break; // INT3
        case 21: interruptNum = 2; break; // INT2
        default:
            return false; // Invalid pin for external interrupt
    }
    
#elif defined(ESP32) || defined(ARDUINO_ARCH_ESP32)
    // ESP32 - Any GPIO can be used for interrupt
    uint8_t interruptNum = digitalPinToInterrupt(_irSensorPin);
    if (interruptNum == NOT_AN_INTERRUPT) {
        return false;
    }
    
#elif defined(__arm__) && defined(TEENSYDUINO)
    // Teensy 4.x - Most pins support interrupts
    uint8_t interruptNum = digitalPinToInterrupt(_irSensorPin);
    if (interruptNum == NOT_AN_INTERRUPT) {
        return false;
    }
    
#else
    // Try generic Arduino interrupt mapping
    uint8_t interruptNum = digitalPinToInterrupt(_irSensorPin);
    if (interruptNum == NOT_AN_INTERRUPT) {
        return false;
    }
#endif
    
    // Attach interrupt with rising edge detection
    attachInterrupt(interruptNum, []() {
        if (activeInstance != nullptr) {
            activeInstance->HandlePulseInterrupt();
        }
    }, RISING);
    
    return true;
}


/**
 * Atomic read of 32-bit volatile variable.
 */
uint32_t Tachometer::AtomicRead32(volatile uint32_t& volatileVar) {
    uint32_t result;
    
#if defined(ESP32) || defined(ARDUINO_ARCH_ESP32)
    // ESP32 has 32-bit atomic operations
    portENTER_CRITICAL_ISR(&mux);
    result = volatileVar;
    portEXIT_CRITICAL_ISR(&mux);
    
#elif defined(__arm__) && defined(TEENSYDUINO)
    // Teensy 4.x - ARM Cortex-M7 has atomic 32-bit operations
    __disable_irq();
    result = volatileVar;
    __enable_irq();
    
#else
    // AVR and other platforms
    noInterrupts();
    result = volatileVar;
    interrupts();
#endif
    
    return result;
}


/**
 * Atomic write to 32-bit volatile variable.
 */
void Tachometer::AtomicWrite32(volatile uint32_t& volatileVar, uint32_t value) {
#if defined(ESP32) || defined(ARDUINO_ARCH_ESP32)
    // ESP32 has 32-bit atomic operations
    portENTER_CRITICAL_ISR(&mux);
    volatileVar = value;
    portEXIT_CRITICAL_ISR(&mux);
    
#elif defined(__arm__) && defined(TEENSYDUINO)
    // Teensy 4.x - ARM Cortex-M7 has atomic 32-bit operations
    __disable_irq();
    volatileVar = value;
    __enable_irq();
    
#else
    // AVR and other platforms
    noInterrupts();
    volatileVar = value;
    interrupts();
#endif
}


/**
 * Calculate timer compare value for desired period.
 */
uint16_t Tachometer::CalculateTimerCompareValue(uint16_t periodMs) {
#ifdef ARDUINO_ARCH_AVR
    // Formula for CTC mode with prescaler 1024:
    // compareValue = (F_CPU * periodMs) / (1024 * 1000) - 1
    // For 16MHz: compareValue = (16000000 * periodMs) / (1024000) - 1
    uint32_t compareValue = ((F_CPU / 1024UL) * periodMs) / 1000UL - 1;
    
    // Limit to 16-bit maximum
    if (compareValue > 65535) {
        compareValue = 65535;
    }
    
    return (uint16_t)compareValue;
    
#else
    // For non-AVR platforms, this function is not used
    // Return a dummy value to avoid compiler warnings
    return 0;
#endif
}


/**
 * Apply digital filtering to measurements.
 */
void Tachometer::ApplyDigitalFilter() {
    // Apply low-pass filter: filtered = alpha * new + (1-alpha) * filtered
    // Using integer math: alpha is scaled by 1000 (e.g., 0.8 = 800)
    uint32_t alphaScaled = _filterAlpha;
    uint32_t oneMinusAlpha = 1000 - alphaScaled;
    
    // Low-pass filter on frequency
    _filteredFrequency = (alphaScaled * _currentFrequencyHz + oneMinusAlpha * _filteredFrequency) / 1000;
    
    // Low-pass filter on RPM
    _filteredRpm = (alphaScaled * _currentRpm + oneMinusAlpha * _filteredRpm) / 1000;
    
    // Apply moving average if window size > 1
    if (_windowSize > 1) {
        // Add current frequency to circular buffer
        _frequencyHistory[_historyIndex] = _filteredFrequency;
        _historyIndex = (_historyIndex + 1) % _windowSize;
        
        if (_historyCount < _windowSize) {
            _historyCount++;
        }
        
        // Calculate moving average
        uint32_t sum = 0;
        for (uint8_t i = 0; i < _historyCount; i++) {
            sum += _frequencyHistory[i];
        }
        
        _filteredFrequency = sum / _historyCount;
        
        // Recalculate filtered RPM from averaged frequency
        if (_pulsesPerRevolution > 0) {
            _filteredRpm = (_filteredFrequency * 60UL) / _pulsesPerRevolution;
        }
    }
}


// Platform-specific ISR definitions
#ifdef ARDUINO_ARCH_AVR
// Timer interrupt service routines for AVR (Arduino Mega)
ISR(TIMER1_COMPA_vect) {
    if (activeInstance != nullptr && activeInstance->GetTimerNumber() == 1) {
        activeInstance->HandleTimerInterrupt();
    }
}

ISR(TIMER3_COMPA_vect) {
    if (activeInstance != nullptr && activeInstance->GetTimerNumber() == 3) {
        activeInstance->HandleTimerInterrupt();
    }
}

ISR(TIMER4_COMPA_vect) {
    if (activeInstance != nullptr && activeInstance->GetTimerNumber() == 4) {
        activeInstance->HandleTimerInterrupt();
    }
}

ISR(TIMER5_COMPA_vect) {
    if (activeInstance != nullptr && activeInstance->GetTimerNumber() == 5) {
        activeInstance->HandleTimerInterrupt();
    }
}
#endif // ARDUINO_ARCH_AVR