# Sistema Tacômetro Industrial para Arduino/ESP32/Teensy

**Versão:** 1.0  
**Data:** 28 de Junho de 2025  
**Autor:** Omar Achraf <omarachraf@gmail.com>  

---

## Sumário

1. [Introdução](#introdução)
2. [Características Técnicas](#características-técnicas)
3. [Teoria de Operação](#teoria-de-operação)
4. [Configuração de Hardware](#configuração-de-hardware)
5. [Instalação e Uso](#instalação-e-uso)
6. [Exemplos de Código](#exemplos-de-código)
7. [Filtragem Digital](#filtragem-digital)
8. [Compatibilidade de Plataformas](#compatibilidade-de-plataformas)
9. [Solução de Problemas](#solução-de-problemas)
10. [Referências](#referências)
11. [Glossário](#glossário)

---

## Introdução

O Sistema Tacômetro Industrial é uma biblioteca C++ otimizada para medição de alta precisão de velocidade rotacional usando Arduino Mega, ESP32 ou Teensy 4.1. Implementa arquitetura de dupla interrupção com filtragem digital avançada para aplicações industriais críticas.

### Principais Características

- **Arquitetura de Dupla Interrupção**: ISR externa para contagem de pulsos + ISR de timer para base de tempo
- **Operações Apenas com Inteiros**: Otimizado para máxima performance sem operações de ponto flutuante
- **Filtragem Digital Configurável**: Low-pass filter + moving average para redução de ruído
- **Thread-Safe**: Acesso atômico a variáveis compartilhadas entre ISRs
- **Multi-Plataforma**: Compatível com Arduino IDE e PlatformIO
- **Conformidade DO-178C**: Seguindo padrões de desenvolvimento crítico

---

## Características Técnicas

### Especificações de Performance

| Parâmetro | Arduino Mega | ESP32 | Teensy 4.1 |
|-----------|--------------|-------|-------------|
| **Frequência Máxima** | 50 kHz | 200 kHz | 500 kHz |
| **Latência de ISR** | ~4 µs | ~1 µs | ~0.5 µs |
| **Resolução Temporal** | 4 µs (micros()) | 1 µs | 0.1 µs |
| **Precisão Timer** | ±0.01% | ±0.001% | ±0.0001% |
| **Memória RAM** | ~200 bytes | ~300 bytes | ~400 bytes |

### Recursos Suportados

- **Pinos de Interrupção**: 2, 3, 18, 19, 20, 21 (Arduino Mega)
- **Timers Disponíveis**: Timer1, Timer3, Timer4, Timer5
- **Debounce Hardware/Software**: 10µs - 65ms configurável
- **Base de Tempo**: 100ms - 65.535s
- **Filtragem Digital**: Coeficiente 0.001 - 1.000

---

## Teoria de Operação

### Arquitetura de Dupla Interrupção

O sistema utiliza duas interrupções independentes para máxima precisão:

```
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────┐
│   Sensor IR     │───▶│ ISR Externa      │───▶│ Contador Pulsos │
│   (Fototrans.)  │    │ (HandlePulse)    │    │ (pulseCounter_) │
└─────────────────┘    └──────────────────┘    └─────────────────┘
                                 ▲
                                 │ Debounce Filter
                                 │ (debounceMicros_)
                                 ▼
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────┐
│   Timer CTC     │───▶│ ISR Timer        │───▶│ Cálculo RPM     │
│   (1Hz padrão)  │    │ (HandleTimer)    │    │ (currentRpm_)   │
└─────────────────┘    └──────────────────┘    └─────────────────┘
```

### Base de Tempo e Sincronização

**Timer em Modo CTC (Clear Timer on Compare)**:
- **Prescaler 1024**: Reduz clock de 16MHz para 15.625kHz
- **Compare Value**: `(F_CPU * periodo_ms) / (prescaler * 1000) - 1`
- **Precisão**: ±1 clock cycle = ±64µs @ 16MHz

**Fórmulas de Cálculo**:
```cpp
// Frequência em Hz
frequency = (pulsos_contados * 1000) / periodo_amostra_ms

// RPM (Revoluções Por Minuto)  
rpm = (frequency * 60) / pulsos_por_revolucao

// Intervalo entre pulsos
intervalo_us = micros_atual - micros_anterior
```

### Gerenciamento de Memória Thread-Safe

**Problema**: Variáveis de 32 bits não são atômicas em AVR (8-bit)
```cpp
// ❌ PERIGOSO - Pode corromper dados
volatile uint32_t counter;
counter = newValue;  // 4 operações separadas!

// ✅ SEGURO - Acesso atômico
void AtomicWrite32(volatile uint32_t& var, uint32_t value) {
    noInterrupts();  // Bloqueia todas as interrupções
    var = value;     // Operação atômica garantida
    interrupts();    // Reativa interrupções
}
```

---

## Configuração de Hardware

### Circuito de Entrada Recomendado

#### Configuração Básica (Baixo Ruído)
```
                    VCC (5V/3.3V)
                         │
                         ├─── 10kΩ (Pull-up)
                         │
IR Sensor ──────┬────────┼─── Arduino Pin (2,3,18,19,20,21)
                │        │
               ┌─┴─┐    ┌─┴─┐
               │100│    │100│ 100nF (Filtro de ruído)
               │nF │    │Ω  │
               └─┬─┘    └─┬─┘
                 │        │
                ───      ───
                GND      GND
```

#### Configuração Avançada (Alto Ruído Industrial)
```
                    VCC
                     │
                     ├─── 10kΩ
                     │
IR Sensor ──┬─── Optoacoplador ──┬─── 1kΩ ──┬─── Arduino Pin
            │     (4N25/6N137)  │          │
           ┌─┴─┐                ┌─┴─┐      ┌─┴─┐
           │1nF│                │100│      │100│ 
           └─┬─┘                │nF │      │nF │
             │                  └─┬─┘      └─┬─┘
            ───                  ───        ───
            GND                  GND        GND
```

### Recomendações de Cabeamento

#### Cabos e Conectores

| Aplicação | Tipo de Cabo | Comprimento Máximo | Justificativa |
|-----------|--------------|-------------------|---------------|
| **Baixo Ruído** | Cabo par trançado CAT5e | 100m | Cancelamento de modo comum |
| **Médio Ruído** | Cabo blindado + par trançado | 50m | Blindagem + cancelamento |
| **Alto Ruído** | Cabo blindado duplo + ferrite | 20m | Máxima proteção EMI/RFI |
| **Industrial** | Cabo militar spec + fibra óptica | 1km+ | Isolamento galvânico total |

#### Técnicas de Instalação

**1. Roteamento de Cabos**:
```
❌ EVITAR:
- Paralelo a cabos de potência AC
- Próximo a motores/relés
- Loops grandes (antenas)

✅ RECOMENDADO:
- 90° cruzando cabos AC
- Distância mínima 30cm de fontes EMI
- Cabo mais curto possível
- Aterramento em apenas um ponto
```

**2. Aterramento e Blindagem**:
```
Sensor ──── Cabo Blindado ──── Arduino
            │              │
           ┌─┴─┐           ┌─┴─┐
           │360°│          │Não│ ← Aterrar apenas uma extremidade
           │GND │          │   │   para evitar loops de terra
           └───┘          └───┘
```

### Escolha de Sensores

#### Sensores IR Recomendados

| Modelo | Tipo | Frequência Máx | Aplicação | Preço |
|--------|------|----------------|-----------|--------|
| **TCST2103** | Fototransistor | 50kHz | Geral, baixo custo | $ |
| **HOA6299** | Fotodiodo | 200kHz | Alta velocidade | $ |
| **6N137** | Optoacoplador | 1MHz | Industrial, isolado | $$ |
| **HFBR-1414** | Fibra óptica | 5MHz | Ultra alta velocidade | $$ |

#### Configuração do Nível de Interrupção

```cpp
// Configuração para diferentes tipos de sensores
attachInterrupt(interruptNumber, ISR_function, trigger_mode);

// RISING - Para sensores normalmente LOW
// Melhor para: Fototransistores, TTL logic
attachInterrupt(0, sensorISR, RISING);

// FALLING - Para sensores normalmente HIGH  
// Melhor para: Pull-up com chave mecânica
attachInterrupt(0, sensorISR, FALLING);

// CHANGE - Para sensores bi-direcionais
// Melhor para: Encoders quadratura, máxima resolução
attachInterrupt(0, sensorISR, CHANGE);
```

---

## Instalação e Uso

### Instalação via PlatformIO (Recomendado)

**1. Criar projeto PlatformIO**:
```ini
; platformio.ini
[env:megaatmega2560]
platform = atmelavr
board = megaatmega2560
framework = arduino
lib_deps = 
    https://github.com/omarachraf/tachometer-library.git

[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
lib_deps = 
    https://github.com/omarachraf/tachometer-library.git

[env:teensy41]
platform = teensy
board = teensy41
framework = arduino
lib_deps = 
    https://github.com/omarachraf/tachometer-library.git
```

**2. Estrutura do projeto**:
```
projeto_tacometro/
├── platformio.ini
├── src/
│   └── main.cpp
├── lib/
│   └── Tachometer/
│       ├── Tachometer.h
│       └── Tachometer.cpp
└── test/
    └── test_basic_functionality.cpp
```

### Instalação via Arduino IDE

**1. Copiar arquivos**:
```
Arduino/
└── libraries/
    └── Tachometer/
        ├── Tachometer.h
        ├── Tachometer.cpp
        ├── library.properties
        └── examples/
            ├── BasicUsage/
            ├── AdvancedFiltering/
            └── MultipleInstances/
```

**2. Arquivo library.properties**:
```properties
name=Industrial Tachometer
version=1.0.0
author=Omar Achraf <omarachraf@gmail.com>
maintainer=Omar Achraf <omarachraf@gmail.com>
sentence=High-precision tachometer for industrial applications
paragraph=Dual-interrupt architecture with digital filtering for Arduino/ESP32/Teensy
category=Sensors
url=https://github.com/omarachraf/tachometer-library
architectures=avr,esp32,arm
includes=Tachometer.h
```

---

## Exemplos de Código

### Exemplo 1: Uso Básico Sem Filtragem

```cpp
/**
 * File: basic_tachometer.ino
 * Author: Omar Achraf / omarachraf@gmail.com
 * Date: 2025-06-28
 * Description: Exemplo básico de tacômetro sem filtragem digital
 */

#include "Tachometer.h"

// Configuração simples
const uint8_t IR_PIN = 2;           // Pino INT0
const uint16_t SAMPLE_PERIOD = 500; // 500ms para resposta rápida
const uint8_t PULSES_PER_REV = 4;   // Encoder com 4 pulsos/revolução

// Instância do tacômetro (sem filtragem)
Tachometer tach(IR_PIN, SAMPLE_PERIOD, 100, PULSES_PER_REV, 1, false);


void setup() {
    Serial.begin(115200);
    
    if (!tach.Initialize()) {
        Serial.println("ERRO: Falha na inicialização!");
        while(1);
    }
    
    Serial.println("Tacômetro básico iniciado...");
}


void loop() {
    if (tach.IsNewDataAvailable()) {
        Serial.print("RPM: ");
        Serial.print(tach.GetCurrentRpm());
        Serial.print(" | Freq: ");
        Serial.print(tach.GetCurrentFrequencyHz());
        Serial.println(" Hz");
    }
    
    delay(10);
}
```

### Exemplo 2: Configuração Avançada com Filtragem

```cpp
/**
 * File: advanced_filtering.ino
 * Author: Omar Achraf / omarachraf@gmail.com
 * Date: 2025-06-28
 * Description: Tacômetro com filtragem digital para ambientes ruidosos
 */

#include "Tachometer.h"

// Configuração para ambiente industrial (muito ruído)
const uint8_t IR_PIN = 3;             // Pino INT1
const uint16_t SAMPLE_PERIOD = 1000;  // 1 segundo para estabilidade
const uint16_t DEBOUNCE_TIME = 200;   // 200µs debounce agressivo
const uint8_t PULSES_PER_REV = 1;     // Sensor simples
const uint8_t TIMER_NUM = 3;          // Timer3 para não conflitar
const bool ENABLE_FILTER = true;      // Ativar filtragem
const uint16_t FILTER_ALPHA = 700;    // 0.7 - Filtragem moderada
const uint8_t WINDOW_SIZE = 10;       // Média móvel de 10 amostras

// Instância com filtragem completa
Tachometer tach(IR_PIN, SAMPLE_PERIOD, DEBOUNCE_TIME, PULSES_PER_REV, 
               TIMER_NUM, ENABLE_FILTER, FILTER_ALPHA, WINDOW_SIZE);


void setup() {
    Serial.begin(115200);
    Serial.println("=== Tacômetro Industrial com Filtragem ===");
    
    if (!tach.Initialize()) {
        Serial.println("ERRO: Inicialização falhou!");
        while(1);
    }
    
    // Configuração dinâmica de parâmetros
    tach.SetFilterParameters(750, 8);  // Ajuste fino durante runtime
    
    Serial.println("Sistema pronto. Configuração:");
    Serial.print("- Pino: "); Serial.println(IR_PIN);
    Serial.print("- Período: "); Serial.print(SAMPLE_PERIOD); Serial.println("ms");
    Serial.print("- Debounce: "); Serial.print(DEBOUNCE_TIME); Serial.println("µs");
    Serial.print("- Filtragem: "); Serial.println(ENABLE_FILTER ? "ATIVA" : "INATIVA");
}


void loop() {
    if (tach.IsNewDataAvailable()) {
        // Leituras brutas (sem filtro)
        uint32_t rawRpm = tach.GetCurrentRpm();
        uint32_t rawFreq = tach.GetCurrentFrequencyHz();
        
        // Leituras filtradas
        uint32_t filteredRpm = tach.GetFilteredRpm();
        uint32_t filteredFreq = tach.GetFilteredFrequencyHz();
        
        // Dados adicionais
        uint32_t totalRevs = tach.GetTotalRevolutions();
        uint32_t pulseInterval = tach.GetPulseIntervalMicros();
        
        // Saída formatada
        Serial.println("╔════════════════════════════════════════╗");
        Serial.print("║ RPM Bruto:     "); Serial.print(rawRpm); Serial.println(" rpm");
        Serial.print("║ RPM Filtrado:  "); Serial.print(filteredRpm); Serial.println(" rpm");
        Serial.print("║ Freq Bruta:    "); Serial.print(rawFreq); Serial.println(" Hz");
        Serial.print("║ Freq Filtrada: "); Serial.print(filteredFreq); Serial.println(" Hz");
        Serial.print("║ Total Revs:    "); Serial.println(totalRevs);
        Serial.print("║ Intervalo:     "); Serial.print(pulseInterval); Serial.println(" µs");
        Serial.println("╚════════════════════════════════════════╝");
        Serial.println();
    }
    
    // Comando serial para ajuste dinâmico
    if (Serial.available()) {
        String command = Serial.readStringUntil('\n');
        processCommand(command);
    }
    
    delay(10);
}


/**
 * Processa comandos seriais para ajuste dinâmico de parâmetros.
 */
void processCommand(String cmd) {
    cmd.trim();
    cmd.toLowerCase();
    
    if (cmd == "reset") {
        tach.ResetCounters();
        Serial.println("Contadores resetados!");
        
    } else if (cmd.startsWith("filter ")) {
        // Comando: "filter 800 5" (alpha=0.8, window=5)
        int spaceIndex = cmd.indexOf(' ', 7);
        if (spaceIndex > 0) {
            uint16_t alpha = cmd.substring(7, spaceIndex).toInt();
            uint8_t window = cmd.substring(spaceIndex + 1).toInt();
            
            if (tach.SetFilterParameters(alpha, window)) {
                Serial.print("Filtro ajustado: alpha=");
                Serial.print(alpha);
                Serial.print(", window=");
                Serial.println(window);
            } else {
                Serial.println("ERRO: Parâmetros inválidos!");
            }
        }
        
    } else if (cmd == "filter on") {
        tach.SetFilteringEnabled(true);
        Serial.println("Filtragem ATIVADA");
        
    } else if (cmd == "filter off") {
        tach.SetFilteringEnabled(false);
        Serial.println("Filtragem DESATIVADA");
        
    } else if (cmd.startsWith("debounce ")) {
        uint16_t newDebounce = cmd.substring(9).toInt();
        tach.SetDebounceTime(newDebounce);
        Serial.print("Debounce ajustado para: ");
        Serial.print(newDebounce);
        Serial.println(" µs");
        
    } else if (cmd == "help") {
        Serial.println("Comandos disponíveis:");
        Serial.println("- reset               : Zerar contadores");
        Serial.println("- filter <alpha> <win>: Ajustar filtro");
        Serial.println("- filter on/off       : Liga/desliga filtro");
        Serial.println("- debounce <us>       : Ajustar debounce");
        Serial.println("- help                : Esta ajuda");
        
    } else {
        Serial.println("Comando desconhecido. Digite 'help' para ajuda.");
    }
}
```

### Exemplo 3: Múltiplas Instâncias (Sistema Multi-Eixo)

```cpp
/**
 * File: multi_axis_tachometer.ino
 * Author: Omar Achraf / omarachraf@gmail.com
 * Date: 2025-06-28
 * Description: Sistema tacômetro multi-eixo para aplicações complexas
 */

#include "Tachometer.h"

// Configuração para 3 eixos independentes
Tachometer eixoX(2, 1000, 100, 4, 1, true, 800, 5);  // INT0, Timer1
Tachometer eixoY(3, 1000, 100, 4, 3, true, 800, 5);  // INT1, Timer3  
Tachometer eixoZ(18, 1000, 100, 4, 4, true, 800, 5); // INT5, Timer4

// Estrutura para dados consolidados
struct AxisData {
    uint32_t rpm;
    uint32_t frequency;
    uint32_t totalRevs;
    bool dataValid;
};

AxisData axes[3];


void setup() {
    Serial.begin(115200);
    Serial.println("=== Sistema Tacômetro Multi-Eixo ===");
    
    // Inicializar todos os eixos
    bool initOk = true;
    
    if (!eixoX.Initialize()) {
        Serial.println("ERRO: Falha inicialização Eixo X");
        initOk = false;
    }
    
    if (!eixoY.Initialize()) {
        Serial.println("ERRO: Falha inicialização Eixo Y");
        initOk = false;
    }
    
    if (!eixoZ.Initialize()) {
        Serial.println("ERRO: Falha inicialização Eixo Z");
        initOk = false;
    }
    
    if (!initOk) {
        Serial.println("Sistema com falhas! Verifique conexões.");
        while(1);
    }
    
    Serial.println("Todos os eixos inicializados com sucesso!");
    Serial.println("Formato: [Eixo] RPM | Freq | Total");
    Serial.println("========================================");
}


void loop() {
    bool hasNewData = false;
    
    // Coletar dados de todos os eixos
    if (eixoX.IsNewDataAvailable()) {
        axes[0].rpm = eixoX.GetFilteredRpm();
        axes[0].frequency = eixoX.GetFilteredFrequencyHz();
        axes[0].totalRevs = eixoX.GetTotalRevolutions();
        axes[0].dataValid = true;
        hasNewData = true;
    }
    
    if (eixoY.IsNewDataAvailable()) {
        axes[1].rpm = eixoY.GetFilteredRpm();
        axes[1].frequency = eixoY.GetFilteredFrequencyHz();
        axes[1].totalRevs = eixoY.GetTotalRevolutions();
        axes[1].dataValid = true;
        hasNewData = true;
    }
    
    if (eixoZ.IsNewDataAvailable()) {
        axes[2].rpm = eixoZ.GetFilteredRpm();
        axes[2].frequency = eixoZ.GetFilteredFrequencyHz();
        axes[2].totalRevs = eixoZ.GetTotalRevolutions();
        axes[2].dataValid = true;
        hasNewData = true;
    }
    
    // Exibir dados quando houver atualizações
    if (hasNewData) {
        displayMultiAxisData();
        
        // Detectar condições de alarme
        checkAlarmConditions();
    }
    
    delay(10);
}


/**
 * Exibe dados de todos os eixos em formato tabular.
 */
void displayMultiAxisData() {
    static uint32_t displayCounter = 0;
    
    // Header a cada 20 linhas
    if (displayCounter % 20 == 0) {
        Serial.println();
        Serial.println("┌──────┬──────────┬──────────┬──────────┐");
        Serial.println("│ Eixo │   RPM    │   Freq   │   Total  │");
        Serial.println("├──────┼──────────┼──────────┼──────────┤");
    }
    
    // Dados de cada eixo
    const char* eixoNames[] = {"  X  ", "  Y  ", "  Z  "};
    
    for (int i = 0; i < 3; i++) {
        if (axes[i].dataValid) {
            Serial.print("│ ");
            Serial.print(eixoNames[i]);
            Serial.print(" │ ");
            Serial.print(String(axes[i].rpm).substring(0, 8));
            Serial.print(" │ ");
            Serial.print(String(axes[i].frequency).substring(0, 8));
            Serial.print(" │ ");
            Serial.print(String(axes[i].totalRevs).substring(0, 8));
            Serial.println(" │");
            
            axes[i].dataValid = false; // Reset flag
        }
    }
    
    if (displayCounter % 20 == 19) {
        Serial.println("└──────┴──────────┴──────────┴──────────┘");
    }
    
    displayCounter++;
}


/**
 * Verifica condições de alarme para segurança do sistema.
 */
void checkAlarmConditions() {
    const uint32_t MAX_RPM = 5000;        // RPM máximo permitido
    const uint32_t MIN_RPM = 10;          // RPM mínimo (detecção de parada)
    const uint32_t RPM_DIFF_ALARM = 500;  // Diferença máxima entre eixos
    
    // Verificar overspeed
    for (int i = 0; i < 3; i++) {
        if (axes[i].rpm > MAX_RPM) {
            Serial.print("⚠️  ALARME OVERSPEED - Eixo ");
            Serial.print((char)('X' + i));
            Serial.print(": ");
            Serial.print(axes[i].rpm);
            Serial.println(" RPM");
        }
        
        if (axes[i].rpm < MIN_RPM && axes[i].rpm > 0) {
            Serial.print("⚠️  ALARME LOW SPEED - Eixo ");
            Serial.print((char)('X' + i));
            Serial.print(": ");
            Serial.print(axes[i].rpm);
            Serial.println(" RPM");
        }
    }
    
    // Verificar desbalanceamento entre eixos
    uint32_t maxRpm = 0, minRpm = UINT32_MAX;
    for (int i = 0; i < 3; i++) {
        if (axes[i].rpm > maxRpm) maxRpm = axes[i].rpm;
        if (axes[i].rpm < minRpm) minRpm = axes[i].rpm;
    }
    
    if ((maxRpm - minRpm) > RPM_DIFF_ALARM && minRpm > MIN_RPM) {
        Serial.print("⚠️  ALARME DESBALANCEAMENTO: Δ=");
        Serial.print(maxRpm - minRpm);
        Serial.println(" RPM");
    }
}
```

---

## Filtragem Digital

### Teoria dos Filtros Implementados

#### 1. Filtro Passa-Baixas (Low-Pass Filter)

**Equação**: `y[n] = α × x[n] + (1-α) × y[n-1]`

**Onde**:
- `α` = coeficiente do filtro (0.0 - 1.0)
- `x[n]` = valor atual (não filtrado)
- `y[n]` = valor filtrado atual
- `y[n-1]` = valor filtrado anterior

**Implementação Inteira**:
```cpp
// α = 0.8 → α_scaled = 800 (multiplicado por 1000)
uint32_t alpha_scaled = 800;
uint32_t one_minus_alpha = 1000 - alpha_scaled;

filtered_value = (alpha_scaled * raw_value + one_minus_alpha * filtered_value) / 1000;
```

**Características de Resposta**:

| α | Resposta | Ruído | Aplicação |
|---|----------|-------|-----------|
| **0.1** | Muito lenta | Mínimo | Sistema estável, baixo ruído |
| **0.5** | Média | Médio | Geral, boa relação resposta/ruído |
| **0.8** | Rápida | Alto | Sistema dinâmico, resposta rápida |
| **1.0** | Sem filtro | Máximo | Dados brutos |

#### 2. Média Móvel (Moving Average)

**Equação**: `y[n] = (1/N) × Σ(x[n-i])` para i = 0 até N-1

**Implementação com Buffer Circular**:
```cpp
class MovingAverage {
    uint32_t buffer[MAX_WINDOW];
    uint8_t index;
    uint8_t count;
    uint8_t windowSize;
    
public:
    uint32_t addSample(uint32_t newSample) {
        buffer[index] = newSample;
        index = (index + 1) % windowSize;
        if (count < windowSize) count++;
        
        uint32_t sum = 0;
        for (uint8_t i = 0; i < count; i++) {
            sum += buffer[i];
        }
        return sum / count;
    }
};
```

**Efeito do Tamanho da Janela**:

| Janela | Latência | Suavização | Memória | Aplicação |
|--------|----------|------------|---------|-----------|
| **3** | Baixa | Mínima | 12 bytes | Filtragem leve |
| **5** | Média | Boa | 20 bytes | Uso geral |
| **10** | Alta | Excelente | 40 bytes | Sistema estável |
| **20** | Muito alta | Máxima | 80 bytes | Análise científica |

### Configuração Recomendada por Aplicação

```cpp
// Aplicações típicas e configurações recomendadas

// 1. MOTOR ELÉTRICO (baixo ruído, resposta rápida)
Tachometer motorTach(2, 500, 50, 4, 1, true, 900, 3);
//                            │    │   │    │     │   └─ Janela pequena
//                            │    │   │    │     └───── α alto (resposta rápida)
//                            │    │   │    └─────────── Filtragem ativa
//                            │    │   └──────────────── 4 pulsos/rev (encoder)
//                            │    └──────────────────── Debounce baixo
//                            └───────────────────────── 500ms (resposta rápida)

// 2. TURBINA EÓLICA (médio ruído, estabilidade)
Tachometer turbineTach(3, 2000, 200, 1, 3, true, 600, 8);
//                              │     │    │   │     │   └─ Janela média
//                              │     │    │   │     └───── α médio (estabilidade)
//                              │     │    │   └─────────── 1 pulso/rev (simples)
//                              │     │    └───────────────  Debounce alto (ruído)
//                              │     └────────────────────  500ms período longo
//                              └──────────────────────────  2s para estabilidade

// 3. BANCADA DE TESTE (alto ruído, máxima precisão)
Tachometer testBench(18, 1000, 500, 8, 4, true, 400, 15);
//                             │     │    │   │     │    └─ Janela grande
//                             │     │    │   │     └────── α baixo (max filtro)
//                             │     │    │   └──────────── 8 pulsos/rev (alta res)
//                             │     │    └──────────────── Debounce alto
//                             │     └───────────────────── 1s período padrão
//                             └─────────────────────────── Pin especial (INT5)
```

---

## Compatibilidade de Plataformas

### Arduino Mega 2560

**Características**:
- **CPU**: ATmega2560 @ 16MHz
- **Memória**: 8KB SRAM, 256KB Flash
- **Timers**: Timer1, Timer3, Timer4, Timer5 (16-bit)
- **Interrupções**: INT0-INT7 (pinos 2,3,18,19,20,21)

**Limitações**:
- Operações de 32-bit não atômicas
- Clock limitado (máx 50kHz input)
- Memória SRAM restrita

**Otimizações**:
```cpp
#ifdef ARDUINO_AVR_MEGA2560
    // Usar apenas tipos necessários
    typedef uint16_t freq_t;      // Ao invés de uint32_t para freq < 65kHz
    typedef uint8_t window_t;     // Janela máxima 255
    
    // ISR otimizada
    #define FAST_ISR __attribute__((always_inline)) inline
    
    // Reduzir uso de memória
    #define MAX_WINDOW_SIZE 10    // Ao invés de 20
#endif
```

### ESP32

**Características**:
- **CPU**: Dual-core Xtensa @ 240MHz
- **Memória**: 520KB SRAM, 4MB Flash
- **Timers**: 4x 64-bit, precisão de 1µs
- **Interrupções**: Qualquer pino GPIO

**Vantagens**:
- Performance superior (200kHz+ input)
- Mais memória disponível
- Precisão de timing superior
- WiFi/Bluetooth integrado

**Configuração ESP32**:
```cpp
#ifdef ESP32
    // ISR na RAM para máxima velocidade
    #define FAST_ISR IRAM_ATTR
    
    // Usar timer de alta resolução
    hw_timer_t * timer = NULL;
    
    void configureESP32Timer() {
        timer = timerBegin(0, 80, true);  // Prescaler 80 = 1MHz
        timerAttachInterrupt(timer, &onTimer, true);
        timerAlarmWrite(timer, 1000000, true);  // 1 segundo
        timerAlarmEnable(timer);
    }
    
    // Interrupção em qualquer pino
    void setupGPIOInterrupt(uint8_t pin) {
        pinMode(pin, INPUT_PULLUP);
        attachInterrupt(digitalPinToInterrupt(pin), sensorISR, RISING);
    }
#endif
```

### Teensy 4.1

**Características**:
- **CPU**: ARM Cortex-M7 @ 600MHz
- **Memória**: 1MB SRAM, 8MB Flash
- **Timers**: Multiple 32-bit timers
- **Interrupções**: Qualquer pino + prioritização

**Performance Superior**:
- Input até 500kHz+ 
- Latência de ISR < 0.5µs
- Operações atômicas nativas de 32-bit
- Ethernet integrado

**Otimizações Teensy**:
```cpp
#ifdef TEENSY41
    // Usar tipos nativos de 32-bit
    typedef uint32_t atomic_t;
    
    // ISR com prioridade alta
    #define FAST_ISR __attribute__((always_inline)) inline
    
    void setupTeensyInterrupt(uint8_t pin) {
        pinMode(pin, INPUT_PULLUP);
        attachInterrupt(digitalPinToInterrupt(pin), sensorISR, RISING);
        
        // Configurar prioridade da interrupção
        NVIC_SET_PRIORITY(IRQ_GPIO1, 0);  // Prioridade máxima
    }
    
    // Timer de alta precisão
    IntervalTimer precisionTimer;
    
    void startPrecisionTimer(uint32_t microseconds) {
        precisionTimer.begin(timerISR, microseconds);
        precisionTimer.priority(0);  // Prioridade máxima
    }
#endif
```

### Comparação de Performance

| Métrica | Arduino Mega | ESP32 | Teensy 4.1 |
|---------|--------------|-------|-------------|
| **Max Input Freq** | 50 kHz | 200 kHz | 500 kHz |
| **ISR Latency** | ~4 µs | ~1 µs | ~0.5 µs |