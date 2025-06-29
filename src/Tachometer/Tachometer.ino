/**
 * File: TachometerSimple.ino
 * Author: Omar Achraf / omarachraf@gmail.com  
 * Date: 2025-06-28
 * Version: 1.0
 * Description: Simplified Arduino sketch for Tachometer with Python GUI interface.
 *              Communicates via Serial using simple command protocol for external control.
 */

#include "Tachometer.h"

// Configuration constants
const uint8_t  IR_SENSOR_PIN    = 4;     // Use pin 2 (INT0) for IR sensor
const uint16_t SAMPLE_PERIOD_MS = 1000;  // 1 second sample period
const uint16_t DEBOUNCE_MICROS  = 100;   // 100 microsecond debounce
const uint8_t  PULSES_PER_REV   = 1;     // 1 pulse per revolution
const uint8_t  TIMER_NUMBER     = 1;     // Use Timer1
const bool     ENABLE_FILTERING = true;  // Enable digital filtering
const uint16_t FILTER_ALPHA     = 800;   // 0.8 filter coefficient
const uint8_t  WINDOW_SIZE      = 5;     // 5-sample moving average

// Global tachometer instance
Tachometer tach(IR_SENSOR_PIN, SAMPLE_PERIOD_MS, DEBOUNCE_MICROS, 
               PULSES_PER_REV, TIMER_NUMBER, ENABLE_FILTERING, 
               FILTER_ALPHA, WINDOW_SIZE);

// Communication variables
unsigned long lastDataSend = 0;
const unsigned long DATA_SEND_INTERVAL = 100; // Send data every 100ms
bool systemReady = false;


#define RUNTIME

#ifdef RUNTIME

/**
 * Arduino setup function.
 */
void setup() {
    // Initialize serial communication
    Serial.begin(115200);
    
    // Wait for serial connection
    while (!Serial && millis() < 3000) {
        delay(10);
    }
    
    // Send startup message
    Serial.println("TACH:STARTUP");
    delay(100);
    
    // Initialize tachometer
    if (tach.Initialize()) {
        Serial.println("TACH:INIT_OK");
        systemReady = true;
    } else {
        Serial.println("TACH:INIT_ERROR");
        systemReady = false;
    }
    
    // Send initial configuration
    sendConfiguration();
}


/**
 * Arduino main loop.
 */
void loop() {
    // Send data periodically
    if (systemReady && (millis() - lastDataSend >= DATA_SEND_INTERVAL)) {
        sendMeasurementData();
        lastDataSend = millis();
    }
    
    // Process incoming commands
    if (Serial.available()) {
        processSerialCommand();
    }
    
    delay(5);
}

#endif


/**
 * Send current measurement data to Python GUI.
 */
void sendMeasurementData() {
    if (tach.IsNewDataAvailable()) {
        // Get all measurement values
        uint32_t frequency = tach.GetCurrentFrequencyHz();
        uint32_t rpm = tach.GetCurrentRpm();
        uint32_t filteredFreq = tach.GetFilteredFrequencyHz();
        uint32_t filteredRpm = tach.GetFilteredRpm();
        uint32_t totalRevs = tach.GetTotalRevolutions();
        uint32_t rawPulses = tach.GetRawPulseCount();
        uint32_t pulseInterval = tach.GetPulseIntervalMicros();
        
        // Send data in structured format
        Serial.print("DATA:");
        Serial.print(frequency);        Serial.print(",");
        Serial.print(rpm);              Serial.print(",");
        Serial.print(filteredFreq);     Serial.print(",");
        Serial.print(filteredRpm);      Serial.print(",");
        Serial.print(totalRevs);        Serial.print(",");
        Serial.print(rawPulses);        Serial.print(",");
        Serial.print(pulseInterval);    Serial.print(",");
        Serial.print(millis());
        Serial.println();
    }
}


/**
 * Send current configuration to Python GUI.
 */
void sendConfiguration() {
    Serial.print("CONFIG:");
    Serial.print(IR_SENSOR_PIN);       Serial.print(",");
    Serial.print(SAMPLE_PERIOD_MS);    Serial.print(",");
    Serial.print(DEBOUNCE_MICROS);     Serial.print(",");
    Serial.print(PULSES_PER_REV);      Serial.print(",");
    Serial.print(TIMER_NUMBER);        Serial.print(",");
    Serial.print(ENABLE_FILTERING ? 1 : 0);  Serial.print(",");
    Serial.print(FILTER_ALPHA);        Serial.print(",");
    Serial.print(WINDOW_SIZE);
    Serial.println();
}


/**
 * Process commands received from Python GUI.
 */
void processSerialCommand() {
    String command = Serial.readStringUntil('\n');
    command.trim();
    
    if (command.startsWith("CMD:")) {
        String cmd = command.substring(4);
        executeCommand(cmd);
    }
}


/**
 * Execute specific command and send response.
 */
void executeCommand(String cmd) {
    if (cmd == "RESET_ALL") {
        tach.ResetCounters();
        Serial.println("RESP:RESET_ALL_OK");
        
    } else if (cmd == "RESET_FILTERS") {
        tach.ResetFilters();
        Serial.println("RESP:RESET_FILTERS_OK");
        
    } else if (cmd == "RESET_REVS") {
        tach.ResetRevolutionCounters();
        Serial.println("RESP:RESET_REVS_OK");
        
    } else if (cmd == "RESET_SYSTEM") {
        tach.ResetSystem();
        Serial.println("RESP:RESET_SYSTEM_OK");
        
    } else if (cmd == "TOGGLE_FILTER") {
        bool currentState = (tach.GetFilteredFrequencyHz() > 0);
        tach.SetFilteringEnabled(!currentState);
        Serial.print("RESP:FILTER_");
        Serial.println(!currentState ? "ON" : "OFF");
        
    } else if (cmd.startsWith("SET_ALPHA:")) {
        uint16_t alpha = cmd.substring(10).toInt();
        if (alpha <= 1000) {
            tach.SetFilterParameters(alpha, WINDOW_SIZE);
            Serial.print("RESP:ALPHA_SET:");
            Serial.println(alpha);
        } else {
            Serial.println("RESP:ALPHA_ERROR:INVALID_RANGE");
        }
        
    } else if (cmd.startsWith("SET_WINDOW:")) {
        uint8_t window = cmd.substring(11).toInt();
        if (window >= 1 && window <= 20) {
            tach.SetFilterParameters(FILTER_ALPHA, window);
            Serial.print("RESP:WINDOW_SET:");
            Serial.println(window);
        } else {
            Serial.println("RESP:WINDOW_ERROR:INVALID_RANGE");
        }
        
    } else if (cmd.startsWith("SET_DEBOUNCE:")) {
        uint16_t debounce = cmd.substring(13).toInt();
        tach.SetDebounceTime(debounce);
        Serial.print("RESP:DEBOUNCE_SET:");
        Serial.println(debounce);
        
    } else if (cmd.startsWith("SET_PERIOD:")) {
        uint16_t period = cmd.substring(11).toInt();
        if (tach.SetSamplePeriod(period)) {
            Serial.print("RESP:PERIOD_SET:");
            Serial.println(period);
        } else {
            Serial.println("RESP:PERIOD_ERROR:INVALID_RANGE");
        }
        
    } else if (cmd == "GET_CONFIG") {
        sendConfiguration();
        
    } else if (cmd == "GET_STATUS") {
        Serial.print("STATUS:");
        Serial.print(systemReady ? "READY" : "ERROR");
        Serial.print(",");
        Serial.print(millis());
        Serial.println();
        
    } else if (cmd == "PING") {
        Serial.println("RESP:PONG");
        
    } else {
        Serial.print("RESP:UNKNOWN_CMD:");
        Serial.println(cmd);
    }
}


#ifdef TEST

const int ledPin = 2;
const int btnPin = 4;
int counter = 0;

void setup() {
    Serial.begin(115200);
    pinMode(ledPin, OUTPUT);
    pinMode(btnPin, INPUT_PULLUP);
    digitalWrite(ledPin, LOW);
}

void loop() {
    digitalWrite(ledPin, LOW);
    delay(250);
    digitalWrite(ledPin, HIGH);
    delay(250);
    Serial.println(counter);
    counter++;

    int buttonState = digitalRead(btnPin); // Lê o estado do botão
    if (buttonState == LOW) {                 // Botão pressionado (LOW devido ao pull-up)
        digitalWrite(ledPin, HIGH);             // Liga o LED
        Serial.println("Botão pressionado!");
    } else {
        digitalWrite(ledPin, LOW);              // Desliga o LED
        Serial.println("Botão solto.");
    }    
}

#endif