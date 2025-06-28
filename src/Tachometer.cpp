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
    irSensorPin_(irPin),
    samplePeriodMs_(samplePeriodMs),
    debounceMicros_(debounceMicros),
    pulsesPerRevolution_(pulsesPerRev),
    timerNumber_(timerNum),
    filteringEnabled_(enableFiltering),
    filterAlpha_(filterAlpha),
    windowSize_(windowSize),
    filteredFrequency_(0),
    filteredRpm_(0),
    historyIndex_(0),
    historyCount_(0),
    pulseCounter_(0),
    pulsesPerSample_(0),
    lastInterruptMicros_(0),
    previousInterruptMicros_(0),
    pulseIntervalMicros_(0),
    newDataAvailable_(false),
    currentFrequencyHz_(0),
    currentRpm_(0),
    totalRevolutions_(0),
    systemInitialized_(false) {
    
    // Validate timer number for Arduino Mega
    if (timerNumber_ != 1 && timerNumber_ != 3 && timerNumber_ != 4 && timerNumber_ != 5) {
        timerNumber_ = 1; // Default to Timer1 if invalid
    }
    
    // Validate filter parameters
    if (filterAlpha_ > 1000) {
        filterAlpha_ = 1000; // Maximum value for no filtering
    }
    if (windowSize_ < 1 || windowSize_ > 20) {
        windowSize_ = 5; // Default safe value
    }
    
    // Initialize history buffer
    for (uint8_t i = 0; i < 20; i++) {
        frequencyHistory_[i] = 0;
    }
}


/**
 * Initialize hardware and interrupt configuration.
 */
bool Tachometer::Initialize() {
    if (systemInitialized_) {
        return true;
    }
    
    // Set static instance pointer for ISR callbacks
    activeInstance = this;
    
    // Configure IR sensor pin as input with pullup
    pinMode(irSensorPin_, INPUT_PULLUP);
    
    // Initialize timing variables
    lastInterruptMicros_ = micros();
    
    // Configure timer interrupt
    if (!ConfigureTimer()) {
        return false;
    }
    
    // Configure external interrupt
    if (!ConfigureExternalInterrupt()) {
        return false;
    }
    
    systemInitialized_ = true;
    return true;
}


/**
 * Get current frequency in Hz with atomic access protection.
 */
uint32_t Tachometer::GetCurrentFrequencyHz() {
    return AtomicRead32(reinterpret_cast<volatile uint32_t&>(currentFrequencyHz_));
}


/**
 * Get current RPM with atomic access protection.
 */
uint32_t Tachometer::GetCurrentRpm() {
    return AtomicRead32(reinterpret_cast<volatile uint32_t&>(currentRpm_));
}


/**
 * Get total revolution count.
 */
uint32_t Tachometer::GetTotalRevolutions() {
    return totalRevolutions_;
}


/**
 * Check for new data availability.
 */
bool Tachometer::IsNewDataAvailable() {
    if (newDataAvailable_) {
        newDataAvailable_ = false;
        return true;
    }
    return false;
}


/**
 * Reset all counters to zero.
 */
void Tachometer::ResetCounters() {
    noInterrupts();
    pulseCounter_ = 0;
    pulsesPerSample_ = 0;
    lastInterruptMicros_ = 0;
    previousInterruptMicros_ = 0;
    pulseIntervalMicros_ = 0;
    totalRevolutions_ = 0;
    currentFrequencyHz_ = 0;
    currentRpm_ = 0;
    newDataAvailable_ = false;
    interrupts();
}


/**
 * Get raw pulse count from last sample.
 */
uint32_t Tachometer::GetRawPulseCount() {
    return AtomicRead32(pulsesPerSample_);
}


/**
 * Get time interval between last two pulses in microseconds.
 */
uint32_t Tachometer::GetPulseIntervalMicros() {
    return AtomicRead32(pulseIntervalMicros_);
}


/**
 * Change sample period during runtime.
 */
bool Tachometer::SetSamplePeriod(uint16_t newPeriodMs) {
    if (newPeriodMs < 100) {
        return false; // Minimum period validation
    }
    
    noInterrupts();
    samplePeriodMs_ = newPeriodMs;
    
    // Recalculate timer settings
    uint16_t compareValue = CalculateTimerCompareValue(samplePeriodMs_);
    
    // Update timer compare register based on timer number
    switch (timerNumber_) {
        case 1:
            OCR1A = compareValue;
            break;
        case 3:
            OCR3A = compareValue;
            break;
        case 4:
            OCR4A = compareValue;
            break;
        case 5:
            OCR5A = compareValue;
            break;
    }
    
    interrupts();
    return true;
}


/**
 * Change debounce time during runtime.
 */
void Tachometer::SetDebounceTime(uint16_t newDebounceMicros) {
    debounceMicros_ = newDebounceMicros;
}


/**
 * Pulse detection interrupt handler with debounce filtering.
 */
void Tachometer::HandlePulseInterrupt() {
    uint32_t currentMicros = micros();
    
    // Debounce filter - ignore pulses too close together
    if (currentMicros - lastInterruptMicros_ >= debounceMicros_) {
        // Calculate interval between pulses
        if (lastInterruptMicros_ > 0) { // Skip first pulse (no previous reference)
            pulseIntervalMicros_ = currentMicros - lastInterruptMicros_;
        }
        
        // Update timing references
        previousInterruptMicros_ = lastInterruptMicros_;
        lastInterruptMicros_ = currentMicros;
        
        // Increment pulse counter
        pulseCounter_++;
    }
}


/**
 * Timer interrupt handler for frequency calculation with optional filtering.
 */
void Tachometer::HandleTimerInterrupt() {
    // Atomic read and reset of pulse counter
    uint32_t currentPulses = pulseCounter_;
    pulseCounter_ = 0;
    
    // Store pulse count for this sample period
    pulsesPerSample_ = currentPulses;
    
    // Calculate frequency in Hz (pulses per second)
    // Formula: frequency = (pulses * 1000) / samplePeriodMs
    currentFrequencyHz_ = (currentPulses * 1000UL) / samplePeriodMs_;
    
    // Calculate RPM based on frequency and pulses per revolution
    // Formula: RPM = (frequency * 60) / pulsesPerRevolution
    if (pulsesPerRevolution_ > 0) {
        currentRpm_ = (currentFrequencyHz_ * 60UL) / pulsesPerRevolution_;
    }
    
    // Apply digital filtering if enabled
    if (filteringEnabled_) {
        ApplyDigitalFilter();
    }
    
    // Update total revolution counter
    if (pulsesPerRevolution_ > 0) {
        totalRevolutions_ += currentPulses / pulsesPerRevolution_;
    }
    
    // Signal new data availability
    newDataAvailable_ = true;
}


/**
 * Configure timer for precise interrupt generation.
 */
bool Tachometer::ConfigureTimer() {
    uint16_t compareValue = CalculateTimerCompareValue(samplePeriodMs_);
    
    switch (timerNumber_) {
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
    
    return true;
}


/**
 * Configure external interrupt for pulse detection.
 */
bool Tachometer::ConfigureExternalInterrupt() {
    // Map pin to interrupt number (Arduino Mega specific)
    uint8_t interruptNum;
    
    switch (irSensorPin_) {
        case 2:  interruptNum = 0; break; // INT0
        case 3:  interruptNum = 1; break; // INT1
        case 18: interruptNum = 5; break; // INT5
        case 19: interruptNum = 4; break; // INT4
        case 20: interruptNum = 3; break; // INT3
        case 21: interruptNum = 2; break; // INT2
        default:
            return false; // Invalid pin for external interrupt
    }
    
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
    noInterrupts();
    result = volatileVar;
    interrupts();
    return result;
}


/**
 * Atomic write to 32-bit volatile variable.
 */
void Tachometer::AtomicWrite32(volatile uint32_t& volatileVar, uint32_t value) {
    noInterrupts();
    volatileVar = value;
    interrupts();
}


/**
 * Calculate timer compare value for desired period.
 */
uint16_t Tachometer::CalculateTimerCompareValue(uint16_t periodMs) {
    // Formula for CTC mode with prescaler 1024:
    // compareValue = (F_CPU * periodMs) / (1024 * 1000) - 1
    // For 16MHz: compareValue = (16000000 * periodMs) / (1024000) - 1
    uint32_t compareValue = ((F_CPU / 1024UL) * periodMs) / 1000UL - 1;
    
    // Limit to 16-bit maximum
    if (compareValue > 65535) {
        compareValue = 65535;
    }
    
    /**
 * Apply digital filtering to measurements.
 */
void Tachometer::ApplyDigitalFilter() {
    // Apply low-pass filter: filtered = alpha * new + (1-alpha) * filtered
    // Using integer math: alpha is scaled by 1000 (e.g., 0.8 = 800)
    uint32_t alphaScaled = filterAlpha_;
    uint32_t oneMinusAlpha = 1000 - alphaScaled;
    
    // Low-pass filter on frequency
    filteredFrequency_ = (alphaScaled * currentFrequencyHz_ + oneMinusAlpha * filteredFrequency_) / 1000;
    
    // Low-pass filter on RPM
    filteredRpm_ = (alphaScaled * currentRpm_ + oneMinusAlpha * filteredRpm_) / 1000;
    
    // Apply moving average if window size > 1
    if (windowSize_ > 1) {
        // Add current frequency to circular buffer
        frequencyHistory_[historyIndex_] = filteredFrequency_;
        historyIndex_ = (historyIndex_ + 1) % windowSize_;
        
        if (historyCount_ < windowSize_) {
            historyCount_++;
        }
        
        // Calculate moving average
        uint32_t sum = 0;
        for (uint8_t i = 0; i < historyCount_; i++) {
            sum += frequencyHistory_[i];
        }
        
        filteredFrequency_ = sum / historyCount_;
        
        // Recalculate filtered RPM from averaged frequency
        if (pulsesPerRevolution_ > 0) {
            filteredRpm_ = (filteredFrequency_ * 60UL) / pulsesPerRevolution_;
        }
    }
}


// Timer interrupt service routines
ISR(TIMER1_COMPA_vect) {
    if (activeInstance != nullptr && activeInstance->timerNumber_ == 1) {
        activeInstance->HandleTimerInterrupt();
    }
}


ISR(TIMER3_COMPA_vect) {
    if (activeInstance != nullptr && activeInstance->timerNumber_ == 3) {
        activeInstance->HandleTimerInterrupt();
    }
}


ISR(TIMER4_COMPA_vect) {
    if (activeInstance != nullptr && activeInstance->timerNumber_ == 4) {
        activeInstance->HandleTimerInterrupt();
    }
}


ISR(TIMER5_COMPA_vect) {
    if (activeInstance != nullptr && activeInstance->timerNumber_ == 5) {
        activeInstance->HandleTimerInterrupt();
    }
}
