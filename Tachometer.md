# Sistema Tacômetro Industrial para Arduino/ESP32/Teensy

**Versão:** 1.17  
**Data:** 23 de Novembro de 2023  
**Autor:** Omar Achraf <omarachraf@gmail.com>  | [www.uvsbr.com.br](www.uvsbr.com.br)

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
9. [Interface GUI Python](#interface-gui-python)
10. [Protocolo de Comunicação Serial](#protocolo-de-comunicação-serial)
11. [Arquitetura de Software](#arquitetura-de-software)
12. [Testes e Validação](#testes-e-validação)
13. [Solução de Problemas](#solução-de-problemas)
14. [Referências](#referências)
15. [Glossário](#glossário)

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
- **Interface GUI Python**: Sistema completo de monitoramento e controle
- **Protocolo Serial Robusto**: Comunicação confiável com checksum e timeout

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
| **ISR Frequency** | 1-10 Hz | 1-100 Hz | 1-1000 Hz |

### Recursos Suportados

- **Pinos de Interrupção**: 2, 3, 18, 19, 20, 21 (Arduino Mega)
- **Timers Disponíveis**: Timer1, Timer3, Timer4, Timer5
- **Debounce Hardware/Software**: 10µs - 65ms configurável
- **Base de Tempo**: 100ms - 65.535s
- **Filtragem Digital**: Coeficiente 0.001 - 1.000
- **Comunicação Serial**: 9600 - 2000000 bps
- **Múltiplas Instâncias**: Até 6 tacômetros simultâneos

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
| **HOA6299** | Fotodiodo | 200kHz | Alta velocidade | $$ |
| **6N137** | Optoacoplador | 1MHz | Industrial, isolado | $$$ |
| **HFBR-1414** | Fibra óptica | 5MHz | Ultra alta velocidade | $$$$ |

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
//                              │     └────────────────────  2s período longo
//                              └──────────────────────────  Para estabilidade

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
| **Timer Resolution** | 4 µs | 1 µs | 0.1 µs |
| **Memory Usage** | 200 bytes | 300 bytes | 400 bytes |
| **Max Instances** | 4 | 8 | 12 |
| **WiFi/BT** | ❌ | ✅ | ❌ |
| **Ethernet** | ❌ | ❌ | ✅ |

---

## Interface GUI Python

### Visão Geral do Sistema GUI

O sistema inclui uma interface gráfica Python completa para monitoramento e controle em tempo real. A GUI oferece:

- **Monitoramento em Tempo Real**: Displays digitais e gráficos dinâmicos
- **Controle de Parâmetros**: Ajuste de filtros, debounce e período de amostragem
- **Logging Avançado**: Sistema de logs com níveis e exportação
- **Visualização de Dados**: Gráficos matplotlib integrados
- **Comunicação Robusta**: Protocolo serial com verificação de erros

### Características da GUI

#### Principais Funcionalidades

```python
# Estrutura principal da aplicação
class TachometerGUI:
    - Real-time data display (RPM, frequency, revolutions)
    - Live plotting with matplotlib
    - Serial communication management
    - Parameter adjustment controls
    - System logging and export
    - Multi-tab interface organization
```

#### Abas da Interface

**1. Monitor em Tempo Real**:
- Status da conexão serial
- Displays digitais das medições
- Gráficos dinâmicos (RPM vs Tempo, Frequência vs Tempo)
- Indicadores visuais de estado

**2. Controle do Sistema**:
- Botões de reset (all, filters, revolutions, system)
- Controle de filtragem (on/off, parâmetros)
- Ajuste de alpha filter (0-1000)
- Controle de window size (1-20)

**3. Configuração**:
- Ajuste de debounce time
- Modificação do período de amostragem
- Visualização da configuração atual
- Aplicação de mudanças em tempo real

**4. Log do Sistema**:
- Mensagens categorizadas por nível
- Auto-scroll configurável
- Exportação para arquivo
- Limpeza de logs

### Instalação da GUI

#### Dependências Python

```bash
# Instalação via pip
pip install tkinter matplotlib pyserial

# Para sistemas Linux/Ubuntu
sudo apt-get install python3-tk python3-matplotlib python3-serial

# Verificação das dependências
python -c "import tkinter, matplotlib, serial; print('All dependencies OK')"
```

#### Arquivo requirements.txt

```txt
# requirements.txt para GUI
matplotlib>=3.5.0
pyserial>=3.5
```

### Execução da GUI

```bash
# Executar a interface
python TachometerGUI.py

# Ou com argumentos (futura implementação)
python TachometerGUI.py --port COM3 --baudrate 115200
```

### Configuração da Comunicação

#### Detecção Automática de Portas

```python
def refresh_ports(self):
    """Detecta automaticamente portas seriais disponíveis."""
    ports = serial.tools.list_ports.comports()
    port_list = [port.device for port in ports]
    
    # Filtrar apenas portas Arduino/ESP32
    arduino_ports = []
    for port in ports:
        if any(keyword in port.description.lower() 
               for keyword in ['arduino', 'esp32', 'teensy', 'usb']):
            arduino_ports.append(port.device)
    
    return arduino_ports if arduino_ports else port_list
```

#### Configurações de Conexão

| Parâmetro | Valor | Descrição |
|-----------|-------|-----------|
| **Baud Rate** | 115200 | Taxa padrão para alta velocidade |
| **Timeout** | 1 segundo | Timeout para leitura serial |
| **Reconnect** | Automático | Reconexão em caso de falha |
| **Buffer Size** | 1024 bytes | Buffer interno de comunicação |

---

## Protocolo de Comunicação Serial

### Formato das Mensagens

O sistema utiliza um protocolo serial baseado em texto com prefixos identificadores:

#### Mensagens do Arduino para PC

```
DATA:<freq>,<rpm>,<filtered_freq>,<filtered_rpm>,<total_revs>,<raw_pulses>,<pulse_interval>,<timestamp>
CONFIG:<ir_pin>,<sample_period>,<debounce>,<pulses_per_rev>,<timer>,<filtering>,<alpha>,<window>
RESP:<response_text>
TACH:<system_message>
STATUS:<status>,<timestamp>
```

#### Comandos do PC para Arduino

```
CMD:<command_name>
CMD:<command_name>:<parameter>
CMD:<command_name>:<param1>:<param2>
```

### Lista Completa de Comandos

#### Comandos de Reset

| Comando | Função | Resposta |
|---------|--------|----------|
| `CMD:RESET_ALL` | Reset completo de contadores | `RESP:RESET_ALL_OK` |
| `CMD:RESET_FILTERS` | Reset apenas filtros | `RESP:RESET_FILTERS_OK` |
| `CMD:RESET_REVS` | Reset contadores de revolução | `RESP:RESET_REVS_OK` |
| `CMD:RESET_SYSTEM` | Reset sistema completo | `RESP:RESET_SYSTEM_OK` |

#### Comandos de Configuração

| Comando | Parâmetros | Função | Resposta |
|---------|------------|--------|----------|
| `CMD:SET_ALPHA:<value>` | 0-1000 | Ajustar filtro alpha | `RESP:ALPHA_SET:<value>` |
| `CMD:SET_WINDOW:<value>` | 1-20 | Ajustar janela moving average | `RESP:WINDOW_SET:<value>` |
| `CMD:SET_DEBOUNCE:<value>` | µs | Ajustar tempo de debounce | `RESP:DEBOUNCE_SET:<value>` |
| `CMD:SET_PERIOD:<value>` | ms | Ajustar período de amostragem | `RESP:PERIOD_SET:<value>` |

#### Comandos de Controle

| Comando | Função | Resposta |
|---------|--------|----------|
| `CMD:TOGGLE_FILTER` | Liga/desliga filtragem | `RESP:FILTER_ON/OFF` |
| `CMD:GET_CONFIG` | Solicitar configuração atual | `CONFIG:<dados>` |
| `CMD:GET_STATUS` | Solicitar status do sistema | `STATUS:<dados>` |
| `CMD:PING` | Teste de comunicação | `RESP:PONG` |

### Implementação do Parser

#### Parser Arduino (C++)

```cpp
void executeCommand(String cmd) {
    const char* header = "executeCommand()";
    Log::trace("%s: Processing command: %s", header, cmd.c_str());
    
    if (cmd == "RESET_ALL") {
        tach.ResetCounters();
        Serial.println("RESP:RESET_ALL_OK");
        Log::info("%s: All counters reset", header);
        
    } else if (cmd == "RESET_FILTERS") {
        tach.ResetFilters();
        Serial.println("RESP:RESET_FILTERS_OK");
        Log::info("%s: Filters reset", header);
        
    } else if (cmd.startsWith("SET_ALPHA:")) {
        uint16_t alpha = cmd.substring(10).toInt();
        if (alpha <= 1000) {
            tach.SetFilterParameters(alpha, WINDOW_SIZE);
            Serial.print("RESP:ALPHA_SET:");
            Serial.println(alpha);
            Log::info("%s: Alpha set to %d", header, alpha);
        } else {
            Serial.println("RESP:ALPHA_ERROR:INVALID_RANGE");
            Log::warning("%s: Invalid alpha value: %d", header, alpha);
        }
        
    } else if (cmd == "PING") {
        Serial.println("RESP:PONG");
        Log::debug("%s: Ping response sent", header);
        
    } else {
        Serial.print("RESP:UNKNOWN_CMD:");
        Serial.println(cmd);
        Log::warning("%s: Unknown command: %s", header, cmd.c_str());
    }
}
```

#### Parser Python

```python
def process_incoming_data(self, line):
    """
    Processa dados recebidos do Arduino.
    Args:
        line - String da linha recebida via serial
    """
    header = "TachometerGUI.process_incoming_data()"
    
    try:
        if line.startswith("DATA:"):
            # Parse measurement data
            data_str = line[5:]
            values = data_str.split(',')
            
            if len(values) >= 8:
                self.current_data = {
                    'frequency': int(values[0]),
                    'rpm': int(values[1]),
                    'filtered_freq': int(values[2]),
                    'filtered_rpm': int(values[3]),
                    'total_revs': int(values[4]),
                    'raw_pulses': int(values[5]),
                    'pulse_interval': int(values[6]),
                    'timestamp': int(values[7])
                }
                self.data_queue.put(('DATA', self.current_data))
                
        elif line.startswith("CONFIG:"):
            # Parse configuration
            config_str = line[7:]
            values = config_str.split(',')
            
            if len(values) >= 8:
                self.config = {
                    'ir_pin': int(values[0]),
                    'sample_period': int(values[1]),
                    'debounce_time': int(values[2]),
                    'pulses_per_rev': int(values[3]),
                    'timer_number': int(values[4]),
                    'filtering_enabled': bool(int(values[5])),
                    'filter_alpha': int(values[6]),
                    'window_size': int(values[7])
                }
                self.data_queue.put(('CONFIG', self.config))
                
        elif line.startswith("RESP:"):
            response = line[5:]
            self.data_queue.put(('RESPONSE', response))
            
        elif line.startswith("TACH:"):
            message = line[5:]
            self.data_queue.put(('SYSTEM', message))
            
    except Exception as e:
        self.log_message(f"Data parsing error: {str(e)}", "ERROR")
```

### Tratamento de Erros

#### Timeouts e Reconexão

```python
class SerialManager:
    def __init__(self, port, baudrate, timeout=1.0):
        self.port = port
        self.baudrate = baudrate
        self.timeout = timeout
        self.reconnect_attempts = 0
        self.max_reconnect_attempts = 5
        
    def robust_send_command(self, command, max_retries=3):
        """Envia comando com retry automático."""
        for attempt in range(max_retries):
            try:
                self.serial_port.write(f"CMD:{command}\n".encode())
                self.serial_port.flush()
                
                # Aguardar resposta com timeout
                response = self.wait_for_response(timeout=2.0)
                if response and not response.startswith("RESP:ERROR"):
                    return response
                    
            except serial.SerialException as e:
                self.log_error(f"Serial error on attempt {attempt + 1}: {e}")
                if attempt < max_retries - 1:
                    self.attempt_reconnection()
                    
        return None
        
    def attempt_reconnection(self):
        """Tenta reconectar automaticamente."""
        if self.reconnect_attempts < self.max_reconnect_attempts:
            try:
                self.disconnect()
                time.sleep(1)
                self.connect()
                self.reconnect_attempts = 0
                return True
            except:
                self.reconnect_attempts += 1
                return False
        return False
```

---

## Arquitetura de Software

### Diagrama de Classes

```
┌─────────────────────┐
│    Tachometer       │
├─────────────────────┤
│ - _irSensorPin      │
│ - _samplePeriodMs   │
│ - _debounceMicros   │
│ - _pulsesPerRev     │
│ - _timerNumber      │
│ - _filteringEnabled │
│ - volatile counters │
├─────────────────────┤
│ + Initialize()      │
│ + GetCurrentRpm()   │
│ + GetCurrentFreq()  │
│ + SetFilterParams() │
│ + ResetCounters()   │
│ + HandlePulseISR()  │
│ + HandleTimerISR()  │
└─────────────────────┘
           │
           │ uses
           ▼
┌─────────────────────┐
│   FilteringEngine   │
├─────────────────────┤
│ - _filterAlpha      │
│ - _windowSize       │
│ - _historyBuffer[]  │
│ - _filteredValue    │
├─────────────────────┤
│ + ApplyLowPass()    │
│ + ApplyMovingAvg()  │
│ + ResetFilters()    │
└─────────────────────┘
           │
           │ manages
           ▼
┌─────────────────────┐
│   AtomicOperations  │
├─────────────────────┤
│ + AtomicRead32()    │
│ + AtomicWrite32()   │
│ + CriticalSection() │
└─────────────────────┘
```

### Estados do Sistema

```
    ┌─────────────┐
    │ UNINITIALIZED│
    └──────┬──────┘
           │ Initialize()
           ▼
    ┌─────────────┐
    │ INITIALIZING│
    └──────┬──────┘
           │ Success
           ▼
    ┌─────────────┐    Reset    ┌─────────────┐
    │   READY     │◄────────────┤   RUNNING   │
    └──────┬──────┘             └──────┬──────┘
           │ Start                     │
           └───────────────────────────┘
                    │ Error
                    ▼
            ┌─────────────┐
            │    ERROR    │
            └─────────────┘
```

### Fluxo de Dados

```
Sensor IR ──┐
            │ Pulse Edge
            ▼
    ┌─────────────┐     ┌─────────────┐     ┌─────────────┐
    │   Pulse     │────▶│  Debounce   │────▶│   Counter   │
    │    ISR      │     │   Filter    │     │ Increment   │
    └─────────────┘     └─────────────┘     └─────────────┘
                                                    │
Timer ──┐                                          │
        │ 1Hz Interrupt                            │ 
        ▼                                          ▼
┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│   Timer     │────▶│ Frequency   │────▶│   Digital   │
│    ISR      │     │ Calculation │     │  Filtering  │
└─────────────┘     └─────────────┘     └─────────────┘
                                                │
                                                ▼
                                        ┌─────────────┐
                                        │   Output    │
                                        │   Values    │
                                        └─────────────┘
```

### Padrões de Design Implementados

#### 1. Singleton Pattern (para instância ativa)

```cpp
// Static instance para ISR callbacks
static Tachometer* activeInstance = nullptr;

// Na inicialização
bool Tachometer::Initialize() {
    if (activeInstance != nullptr) {
        return false; // Apenas uma instância ativa por vez
    }
    activeInstance = this;
    // ... resto da inicialização
}
```

#### 2. Observer Pattern (para novos dados)

```cpp
class TachometerObserver {
public:
    virtual void onNewData(const TachometerData& data) = 0;
    virtual void onError(const TachometerError& error) = 0;
};

class Tachometer {
    std::vector<TachometerObserver*> observers;
    
public:
    void addObserver(TachometerObserver* observer) {
        observers.push_back(observer);
    }
    
    void notifyObservers(const TachometerData& data) {
        for (auto observer : observers) {
            observer->onNewData(data);
        }
    }
};
```

#### 3. State Pattern (para gerenciamento de estados)

```cpp
enum class TachometerState {
    UNINITIALIZED,
    INITIALIZING,
    READY,
    RUNNING,
    ERROR
};

class StateManager {
public:
    bool transitionTo(TachometerState newState) {
        if (isValidTransition(currentState, newState)) {
            currentState = newState;
            onStateChange(newState);
            return true;
        }
        return false;
    }
    
private:
    TachometerState currentState = TachometerState::UNINITIALIZED;
    
    bool isValidTransition(TachometerState from, TachometerState to) {
        // Implementar matriz de transições válidas
        static const bool transitionMatrix[5][5] = {
            // UN IN RE RU ER
            {0, 1, 0, 0, 1}, // UNINITIALIZED
            {1, 0, 1, 0, 1}, // INITIALIZING  
            {0, 0, 0, 1, 1}, // READY
            {0, 0, 1, 0, 1}, // RUNNING
            {1, 0, 0, 0, 0}  // ERROR
        };
        return transitionMatrix[static_cast<int>(from)][static_cast<int>(to)];
    }
};
```

---

## Testes e Validação

### Estratégia de Testes

#### 1. Testes Unitários

**Teste de Inicialização**:
```cpp
TEST(TachometerTest, InitializationSuccess) {
    Tachometer tach(2, 1000, 100, 1, 1, false);
    
    EXPECT_TRUE(tach.Initialize());
    EXPECT_EQ(tach.GetCurrentRpm(), 0);
    EXPECT_EQ(tach.GetCurrentFrequencyHz(), 0);
    EXPECT_FALSE(tach.IsNewDataAvailable());
}

TEST(TachometerTest, InvalidPinInitialization) {
    Tachometer tach(255, 1000, 100, 1, 1, false); // Pin inválido
    
    EXPECT_FALSE(tach.Initialize());
}
```

**Teste de Filtragem**:
```cpp
TEST(FilteringTest, LowPassFilterBehavior) {
    Tachometer tach(2, 1000, 100, 1, 1, true, 500, 1); // α = 0.5
    tach.Initialize();
    
    // Simular entrada step
    simulateInputPulses(100); // 100 Hz constante
    
    uint32_t filtered = tach.GetFilteredFrequencyHz();
    uint32_t raw = tach.GetCurrentFrequencyHz();
    
    EXPECT_LT(filtered, raw); // Filtrado deve ser menor que raw
    EXPECT_GT(filtered, raw * 0.3); // Mas não muito menor
}
```

#### 2. Testes de Integração

**Teste de Comunicação Serial**:
```cpp
TEST(SerialCommTest, CommandResponseCycle) {
    MockSerial mockSerial;
    TachometerController controller(&mockSerial);
    
    // Enviar comando
    mockSerial.inject("CMD:RESET_ALL\n");
    controller.processSerialInput();
    
    // Verificar resposta
    EXPECT_EQ(mockSerial.getLastOutput(), "RESP:RESET_ALL_OK\n");
}
```

**Teste Multi-Instância**:
```cpp
TEST(MultiInstanceTest, IndependentOperation) {
    Tachometer tach1(2, 1000, 100, 1, 1, false);
    Tachometer tach2(3, 1000, 100, 1, 3, false);
    
    EXPECT_TRUE(tach1.Initialize());
    EXPECT_TRUE(tach2.Initialize());
    
    // Simular pulsos independentes
    simulatePulses(tach1, 50); // 50 Hz no tach1
    simulatePulses(tach2, 75); // 75 Hz no tach2
    
    EXPECT_NEAR(tach1.GetCurrentFrequencyHz(), 50, 2);
    EXPECT_NEAR(tach2.GetCurrentFrequencyHz(), 75, 2);
}
```

#### 3. Testes de Performance

**Benchmark de ISR**:
```cpp
BENCHMARK(TachometerBenchmark, PulseISRLatency) {
    Tachometer tach(2, 1000, 100, 1, 1, false);
    tach.Initialize();
    
    auto start = micros();
    for (int i = 0; i < 1000; i++) {
        tach.HandlePulseInterrupt();
    }
    auto end = micros();
    
    uint32_t avgLatency = (end - start) / 1000;
    EXPECT_LT(avgLatency, 5); // Menos de 5µs por ISR
}
```

**Teste de Throughput**:
```cpp
TEST(PerformanceTest, HighFrequencyInput) {
    Tachometer tach(2, 100, 10, 1, 1, true); // 100ms período, debounce baixo
    tach.Initialize();
    
    // Simular 10kHz por 1 segundo
    simulateHighFrequency(10000, 1000);
    
    uint32_t measured = tach.GetCurrentFrequencyHz();
    EXPECT_NEAR(measured, 10000, 100); // ±1% tolerância
}
```

### Validação em Hardware Real

#### 1. Gerador de Sinais de Teste

**Arduino como Gerador**:
```cpp
/**
 * File: signal_generator.ino
 * Author: Omar Achraf / omarachraf@gmail.com
 * Date: 2025-06-28
 * Description: Gerador de pulsos precisos para teste do tacômetro
 */

// Configuração do gerador
const uint8_t OUTPUT_PIN = 9;          // Pin PWM para saída
const uint32_t BASE_FREQUENCY = 1000;  // 1kHz base
const uint8_t DUTY_CYCLE = 50;         // 50% duty cycle

volatile uint32_t targetFrequency = BASE_FREQUENCY;
volatile bool frequencyChanged = false;


void setup() {
    Serial.begin(115200);
    pinMode(OUTPUT_PIN, OUTPUT);
    
    // Configurar Timer2 para PWM de alta precisão
    setupPrecisionPWM();
    
    Serial.println("Signal Generator Ready");
    Serial.println("Commands: freq <Hz>, sweep <start> <end> <step_ms>");
}


void loop() {
    // Processar comandos seriais
    if (Serial.available()) {
        processCommand();
    }
    
    // Atualizar frequência se necessário
    if (frequencyChanged) {
        updatePWMFrequency(targetFrequency);
        frequencyChanged = false;
        
        Serial.print("Frequency set to: ");
        Serial.print(targetFrequency);
        Serial.println(" Hz");
    }
    
    delay(10);
}


/**
 * Configura Timer2 para PWM de alta precisão.
 */
void setupPrecisionPWM() {
    // Timer2 configuração
    TCCR2A = (1 << COM2A1) | (1 << WGM21) | (1 << WGM20); // Fast PWM
    TCCR2B = (1 << WGM22) | (1 << CS21);                  // Prescaler 8
    
    updatePWMFrequency(BASE_FREQUENCY);
}


/**
 * Atualiza frequência do PWM.
 */
void updatePWMFrequency(uint32_t frequency) {
    // Calcular valores para Timer2
    uint32_t prescaler = 8;
    uint32_t targetCount = (F_CPU / (prescaler * frequency)) - 1;
    
    if (targetCount > 255) {
        // Usar prescaler maior se necessário
        prescaler = 64;
        TCCR2B = (1 << WGM22) | (1 << CS22);
        targetCount = (F_CPU / (prescaler * frequency)) - 1;
    }
    
    if (targetCount <= 255) {
        OCR2A = targetCount;
        OCR2B = targetCount / 2; // 50% duty cycle
    }
}


/**
 * Processa comandos seriais.
 */
void processCommand() {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    
    if (cmd.startsWith("freq ")) {
        uint32_t newFreq = cmd.substring(5).toInt();
        if (newFreq >= 1 && newFreq <= 50000) {
            targetFrequency = newFreq;
            frequencyChanged = true;
        } else {
            Serial.println("Error: Frequency out of range (1-50000 Hz)");
        }
        
    } else if (cmd.startsWith("sweep ")) {
        // Comando: sweep 100 1000 500 (100Hz a 1000Hz, step a cada 500ms)
        int space1 = cmd.indexOf(' ', 6);
        int space2 = cmd.indexOf(' ', space1 + 1);
        
        if (space1 > 0 && space2 > 0) {
            uint32_t startFreq = cmd.substring(6, space1).toInt();
            uint32_t endFreq = cmd.substring(space1 + 1, space2).toInt();
            uint32_t stepTime = cmd.substring(space2 + 1).toInt();
            
            performFrequencySweep(startFreq, endFreq, stepTime);
        }
        
    } else if (cmd == "stop") {
        digitalWrite(OUTPUT_PIN, LOW);
        Serial.println("Output stopped");
        
    } else if (cmd == "help") {
        Serial.println("Available commands:");
        Serial.println("  freq <Hz>           - Set frequency");
        Serial.println("  sweep <start> <end> <step_ms> - Frequency sweep");
        Serial.println("  stop                - Stop output");
        Serial.println("  help                - This help");
        
    } else {
        Serial.println("Unknown command. Type 'help' for commands.");
    }
}


/**
 * Executa varredura de frequência.
 */
void performFrequencySweep(uint32_t startFreq, uint32_t endFreq, uint32_t stepTime) {
    Serial.print("Starting frequency sweep: ");
    Serial.print(startFreq);
    Serial.print(" to ");
    Serial.print(endFreq);
    Serial.print(" Hz, step every ");
    Serial.print(stepTime);
    Serial.println(" ms");
    
    uint32_t currentFreq = startFreq;
    uint32_t step = (endFreq > startFreq) ? 
                   (endFreq - startFreq) / 20 : 
                   (startFreq - endFreq) / 20;
    
    while ((endFreq > startFreq && currentFreq <= endFreq) ||
           (endFreq < startFreq && currentFreq >= endFreq)) {
        
        updatePWMFrequency(currentFreq);
        
        Serial.print("Sweep frequency: ");
        Serial.print(currentFreq);
        Serial.println(" Hz");
        
        delay(stepTime);
        
        // Verificar comando de parada
        if (Serial.available()) {
            String stopCmd = Serial.readStringUntil('\n');
            if (stopCmd.trim() == "stop") {
                Serial.println("Sweep stopped by user");
                break;
            }
        }
        
        currentFreq += (endFreq > startFreq) ? step : -step;
    }
    
    Serial.println("Frequency sweep completed");
}
```

#### 2. Bancada de Teste Automatizada

**Setup de Hardware**:
```
┌─────────────────┐    PWM Signal    ┌─────────────────┐
│   Generator     │──────────────────▶│   Tachometer    │
│   Arduino       │                  │   Under Test    │
└─────────────────┘                  └─────────────────┘
        │                                     │
        │ USB                                 │ USB
        ▼                                     ▼
┌─────────────────┐    Test Control   ┌─────────────────┐
│   Test PC       │◄──────────────────┤   Python GUI    │
│   (Automation)  │                   │   (Monitor)     │
└─────────────────┘                   └─────────────────┘
```

**Script de Teste Automatizado**:
```python
#!/usr/bin/env python3
"""
File: automated_test.py
Author: Omar Achraf / omarachraf@gmail.com
Date: 2025-06-28
Description: Sistema de teste automatizado para validação do tacômetro
"""

import serial
import time
import csv
import json
from datetime import datetime
import matplotlib.pyplot as plt


class TachometerTester:
    """
    Classe responsável por testes automatizados do sistema tacômetro.
    Controla gerador de sinais e monitora resposta do tacômetro para
    validação completa de performance e precisão.
    """
    
    def __init__(self, generator_port, tachometer_port, baudrate=115200):
        header = "TachometerTester.__init__()"
        print(f"{header}: Initializing test system")
        
        self.generator_port = serial.Serial(generator_port, baudrate, timeout=2)
        self.tachometer_port = serial.Serial(tachometer_port, baudrate, timeout=2)
        
        self.test_results = []
        self.start_time = datetime.now()
        
        # Aguardar inicialização
        time.sleep(3)
        
        print(f"{header}: Test system ready")


    def run_accuracy_test(self, test_frequencies, test_duration=10):
        """
        Executa teste de precisão em frequências específicas.
        Args:
            test_frequencies - Lista de frequências para teste
            test_duration - Duração de cada teste em segundos
        Returns:
            dict - Resultados consolidados do teste
        """
        header = "TachometerTester.run_accuracy_test()"
        print(f"{header}: Starting accuracy test")
        
        results = {
            'test_type': 'accuracy',
            'start_time': self.start_time.isoformat(),
            'frequencies': [],
            'measurements': []
        }
        
        for freq in test_frequencies:
            print(f"{header}: Testing frequency {freq} Hz")
            
            # Configurar gerador
            self.generator_port.write(f"freq {freq}\n".encode())
            time.sleep(1)
            
            # Aguardar estabilização
            time.sleep(2)
            
            # Coletar medições
            measurements = []
            start_time = time.time()
            
            while time.time() - start_time < test_duration:
                if self.tachometer_port.in_waiting > 0:
                    line = self.tachometer_port.readline().decode().strip()
                    if line.startswith("DATA:"):
                        data = self.parse_data_line(line)
                        if data:
                            measurements.append(data)
                
                time.sleep(0.1)
            
            # Calcular estatísticas
            if measurements:
                freq_measurements = [m['frequency'] for m in measurements]
                rpm_measurements = [m['rpm'] for m in measurements]
                
                freq_stats = self.calculate_statistics(freq_measurements)
                rpm_stats = self.calculate_statistics(rpm_measurements)
                
                result = {
                    'target_frequency': freq,
                    'measured_frequency': freq_stats,
                    'measured_rpm': rpm_stats,
                    'accuracy_percent': (freq_stats['mean'] / freq) * 100,
                    'sample_count': len(measurements)
                }
                
                results['frequencies'].append(freq)
                results['measurements'].append(result)
                
                print(f"  Target: {freq} Hz, Measured: {freq_stats['mean']:.2f} ± {freq_stats['std']:.2f} Hz")
                print(f"  Accuracy: {result['accuracy_percent']:.2f}%")
        
        # Parar gerador
        self.generator_port.write(b"stop\n")
        
        return results


    def run_response_time_test(self, step_frequencies):
        """
        Testa tempo de resposta a mudanças de frequência.
        Args:
            step_frequencies - Lista de pares (freq_inicial, freq_final)
        Returns:
            dict - Resultados do teste de resposta
        """
        header = "TachometerTester.run_response_time_test()"
        print(f"{header}: Starting response time test")
        
        results = {
            'test_type': 'response_time',
            'steps': []
        }
        
        for initial_freq, final_freq in step_frequencies:
            print(f"{header}: Step test {initial_freq} -> {final_freq} Hz")
            
            # Configurar frequência inicial
            self.generator_port.write(f"freq {initial_freq}\n".encode())
            time.sleep(3)  # Aguardar estabilização
            
            # Marcar tempo e mudar para frequência final
            step_time = time.time()
            self.generator_port.write(f"freq {final_freq}\n".encode())
            
            # Monitorar resposta
            measurements = []
            while time.time() - step_time < 10:  # Máximo 10 segundos
                if self.tachometer_port.in_waiting > 0:
                    line = self.tachometer_port.readline().decode().strip()
                    if line.startswith("DATA:"):
                        data = self.parse_data_line(line)
                        if data:
                            data['elapsed_time'] = time.time() - step_time
                            measurements.append(data)
                            
                            # Verificar se atingiu 95% do valor final
                            if abs(data['frequency'] - final_freq) < (final_freq * 0.05):
                                response_time = time.time() - step_time
                                print(f"  Response time: {response_time:.2f}s")
                                break
                
                time.sleep(0.05)
            
            results['steps'].append({
                'initial_frequency': initial_freq,
                'final_frequency': final_freq,
                'measurements': measurements,
                'response_time': response_time if 'response_time' in locals() else None
            })
        
        return results


    def run_noise_immunity_test(self, base_frequency, noise_levels):
        """
        Testa imunidade a ruído através de modulação.
        Args:
            base_frequency - Frequência base para teste
            noise_levels - Lista de níveis de ruído (0.0 a 1.0)
        Returns:
            dict - Resultados do teste de ruído
        """
        header = "TachometerTester.run_noise_immunity_test()"
        print(f"{header}: Starting noise immunity test")
        
        results = {
            'test_type': 'noise_immunity',
            'base_frequency': base_frequency,
            'noise_tests': []
        }
        
        for noise_level in noise_levels:
            print(f"{header}: Testing noise level {noise_level*100:.0f}%")
            
            # Simular ruído através de variação de frequência
            noise_freq_variation = base_frequency * noise_level * 0.1
            
            measurements = []
            test_start = time.time()
            
            while time.time() - test_start < 30:  # Teste de 30 segundos
                # Variar frequência para simular ruído
                current_time = time.time() - test_start
                noise_component = noise_freq_variation * \
                                 (0.5 - abs((current_time % 2) - 1))
                test_frequency = base_frequency + noise_component
                
                self.generator_port.write(f"freq {int(test_frequency)}\n".encode())
                time.sleep(0.2)
                
                # Coletar medição
                if self.tachometer_port.in_waiting > 0:
                    line = self.tachometer_port.readline().decode().strip()
                    if line.startswith("DATA:"):
                        data = self.parse_data_line(line)
                        if data:
                            measurements.append(data)
            
            # Calcular métricas de ruído
            if measurements:
                frequencies = [m['frequency'] for m in measurements]
                filtered_frequencies = [m['filtered_freq'] for m in measurements]
                
                raw_noise = self.calculate_statistics(frequencies)
                filtered_noise = self.calculate_statistics(filtered_frequencies)
                
                noise_reduction = (raw_noise['std'] - filtered_noise['std']) / raw_noise['std'] * 100
                
                results['noise_tests'].append({
                    'noise_level': noise_level,
                    'raw_statistics': raw_noise,
                    'filtered_statistics': filtered_noise,
                    'noise_reduction_percent': noise_reduction,
                    'sample_count': len(measurements)
                })
                
                print(f"  Noise reduction: {noise_reduction:.1f}%")
        
        return results


    def parse_data_line(self, line):
        """
        Parse linha de dados do tacômetro.
        Args:
            line - String da linha de dados
        Returns:
            dict - Dados parseados ou None se erro
        """
        try:
            data_str = line[5:]  # Remove "DATA:"
            values = data_str.split(',')
            
            if len(values) >= 8:
                return {
                    'frequency': int(values[0]),
                    'rpm': int(values[1]),
                    'filtered_freq': int(values[2]),
                    'filtered_rpm': int(values[3]),
                    'total_revs': int(values[4]),
                    'raw_pulses': int(values[5]),
                    'pulse_interval': int(values[6]),
                    'timestamp': int(values[7])
                }
        except:
            return None
        
        return None


    def calculate_statistics(self, data):
        """
        Calcula estatísticas básicas dos dados.
        Args:
            data - Lista de valores numéricos
        Returns:
            dict - Estatísticas calculadas
        """
        if not data:
            return {'mean': 0, 'std': 0, 'min': 0, 'max': 0}
        
        import statistics
        
        return {
            'mean': statistics.mean(data),
            'std': statistics.stdev(data) if len(data) > 1 else 0,
            'min': min(data),
            'max': max(data)
        }


    def generate_test_report(self, test_results, output_file='test_report.html'):
        """
        Gera relatório HTML com resultados dos testes.
        Args:
            test_results - Lista de resultados de testes
            output_file - Nome do arquivo de saída
        """
        header = "TachometerTester.generate_test_report()"
        print(f"{header}: Generating test report")
        
        html_content = f"""
        <!DOCTYPE html>
        <html>
        <head>
            <title>Tachometer Test Report</title>
            <style>
                body {{ font-family: Arial, sans-serif; margin: 20px; }}
                .header {{ background-color: #f0f0f0; padding: 20px; }}
                .test-section {{ margin: 20px 0; border: 1px solid #ddd; padding: 15px; }}
                .pass {{ color: green; font-weight: bold; }}
                .fail {{ color: red; font-weight: bold; }}
                table {{ border-collapse: collapse; width: 100%; }}
                th, td {{ border: 1px solid #ddd; padding: 8px; text-align: left; }}
                th {{ background-color: #f2f2f2; }}
            </style>
        </head>
        <body>
            <div class="header">
                <h1>Tacômetro Industrial - Relatório de Testes</h1>
                <p>Data: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}</p>
                <p>Autor: Omar Achraf &lt;omarachraf@gmail.com&gt;</p>
            </div>
        """
        
        for test_result in test_results:
            if test_result['test_type'] == 'accuracy':
                html_content += self.generate_accuracy_section(test_result)
            elif test_result['test_type'] == 'response_time':
                html_content += self.generate_response_section(test_result)
            elif test_result['test_type'] == 'noise_immunity':
                html_content += self.generate_noise_section(test_result)
        
        html_content += """
        </body>
        </html>
        """
        
        with open(output_file, 'w', encoding='utf-8') as f:
            f.write(html_content)
        
        print(f"{header}: Report saved to {output_file}")


    def generate_accuracy_section(self, test_result):
        """Gera seção de precisão do relatório."""
        html = '<div class="test-section"><h2>Teste de Precisão</h2>'
        html += '<table><tr><th>Freq. Alvo (Hz)</th><th>Freq. Medida (Hz)</th><th>Precisão (%)</th><th>Desvio Padrão</th><th>Status</th></tr>'
        
        for measurement in test_result['measurements']:
            accuracy = measurement['accuracy_percent']
            status_class = 'pass' if 98 <= accuracy <= 102 else 'fail'
            status_text = 'PASS' if 98 <= accuracy <= 102 else 'FAIL'
            
            html += f'''
            <tr>
                <td>{measurement['target_frequency']}</td>
                <td>{measurement['measured_frequency']['mean']:.2f}</td>
                <td>{accuracy:.2f}</td>
                <td>{measurement['measured_frequency']['std']:.2f}</td>
                <td class="{status_class}">{status_text}</td>
            </tr>
            '''
        
        html += '</table></div>'
        return html


    def close(self):
        """Fecha conexões seriais."""
        self.generator_port.close()
        self.tachometer_port.close()


def main():
    """
    Função principal para execução dos testes automatizados.
    """
    header = "main()"
    print(f"{header}: Starting automated tachometer testing")
    
    try:
        # Configurar portas (ajustar conforme sistema)
        generator_port = "COM3"  # Porta do gerador
        tachometer_port = "COM4"  # Porta do tacômetro
        
        tester = TachometerTester(generator_port, tachometer_port)
        
        # Executar testes
        test_results = []
        
        # Teste de precisão
        accuracy_frequencies = [10, 50, 100, 500, 1000, 2000, 5000]
        accuracy_result = tester.run_accuracy_test(accuracy_frequencies)
        test_results.append(accuracy_result)
        
        # Teste de tempo de resposta
        step_tests = [(100, 1000), (1000, 100), (500, 2000), (2000, 500)]
        response_result = tester.run_response_time_test(step_tests)
        test_results.append(response_result)
        
        # Teste de imunidade a ruído
        noise_levels = [0.1, 0.2, 0.3, 0.5]
        noise_result = tester.run_noise_immunity_test(1000, noise_levels)
        test_results.append(noise_result)
        
        # Gerar relatório
        tester.generate_test_report(test_results)
        
        # Salvar dados brutos
        with open('test_data.json', 'w') as f:
            json.dump(test_results, f, indent=2)
        
        print(f"{header}: All tests completed successfully")
        
    except Exception as e:
        print(f"{header}: Test error - {str(e)}")
    
    finally:
        if 'tester' in locals():
            tester.close()


if __name__ == "__main__":
    main()
```

---

## Solução de Problemas

### Problemas Comuns e Soluções

#### 1. Leituras Instáveis ou Ruidosas

**Sintomas**:
- RPM variando drasticamente
- Valores inconsistentes
- Ruído visível nos gráficos

**Causas Possíveis**:
```
❌ Problemas de Hardware:
- Cabo mal blindado ou longo demais
- Interferência eletromagnética
- Fonte de alimentação ruidosa
- Aterramento inadequado

❌ Problemas de Software:
- Debounce muito baixo
- Período de amostragem muito curto
- Filtragem insuficiente
```

**Soluções**:
```cpp
// 1. Aumentar debounce
tach.SetDebounceTime(200); // Ao invés de 100µs

// 2. Aumentar período de amostragem
tach.SetSamplePeriod(2000); // 2 segundos ao invés de 1

// 3. Ativar filtragem agressiva
tach.SetFilteringEnabled(true);
tach.SetFilterParameters(300, 15); // α=0.3, janela=15

// 4. Verificar aterramento
// - Usar cabo blindado
// - Aterrar blindagem em apenas um ponto
// - Manter distância de fontes EMI
```

#### 2. Leituras Zero ou Ausentes

**Sintomas**:
- GetCurrentRpm() sempre retorna 0
- IsNewDataAvailable() sempre false
- Sem resposta a pulsos

**Diagnóstico**:
```cpp
// Código de diagnóstico
void diagnoseSystem() {
    Serial.println("=== DIAGNÓSTICO DO SISTEMA ===");
    
    // Verificar inicialização
    if (!tach.getInitialized()) {
        Serial.println("ERRO: Sistema não inicializado!");
        return;
    }
    
    // Verificar configuração de pinos
    Serial.print("Pin IR configurado: ");
    Serial.println(IR_SENSOR_PIN);
    
    // Testar entrada digital
    Serial.print("Estado atual do pin: ");
    Serial.println(digitalRead(IR_SENSOR_PIN) ? "HIGH" : "LOW");
    
    // Verificar timer
    Serial.print("Timer configurado: ");
    Serial.println(tach.GetTimerNumber());
    
    // Testar contadores internos
    Serial.print("Contagem de pulsos brutos: ");
    Serial.println(tach.GetRawPulseCount());
    
    Serial.print("Intervalo entre pulsos: ");
    Serial.println(tach.GetPulseIntervalMicros());
}
```

**Soluções**:
```
✅ Hardware:
1. Verificar conexões físicas
2. Testar sensor com multímetro
3. Verificar alimentação (5V/3.3V)
4. Confirmar que pin suporta interrupção

✅ Software:
1. Verificar se Initialize() retorna true
2. Confirmar configuração de pin correto
3. Testar com período menor (100ms)
4. Verificar se timer está livre
```

#### 3. Valores Excessivamente Altos

**Sintomas**:
- RPM impossíveis (>100k)
- Frequência muito alta
- Contadores crescendo muito rápido

**Causas**:
```
❌ Bouncing do sinal
❌ Ruído elétrico sendo contado como pulsos
❌ Debounce muito baixo
❌ Múltiplas bordas por pulso
```

**Soluções**:
```cpp
// 1. Aumentar drasticamente o debounce
tach.SetDebounceTime(1000); // 1ms para sinais muito ruidosos

// 2. Usar filtragem agressiva
tach.SetFilterParameters(100, 20); // α=0.1, janela máxima

// 3. Adicionar filtro de hardware
// Usar optoacoplador ou schmitt trigger

// 4. Verificar tipo de interrupção
// RISING ao invés de CHANGE para evitar double-counting
```

#### 4. Problemas de Comunicação Serial

**Sintomas**:
- GUI não conecta
- Comandos não respondem
- Dados corrompidos

**Diagnóstico Serial**:
```python
def diagnose_serial_communication(port):
    """Diagnostica problemas de comunicação serial."""
    try:
        ser = serial.Serial(port, 115200, timeout=2)
        time.sleep(2)
        
        # Teste básico de ping
        ser.write(b"CMD:PING\n")
        response = ser.readline().decode().strip()
        
        if response == "RESP:PONG":
            print("✅ Comunicação básica OK")
        else:
            print(f"❌ Resposta inesperada: {response}")
        
        # Teste de dados
        data_received = False
        for _ in range(50):  # 5 segundos
            if ser.in_waiting > 0:
                line = ser.readline().decode().strip()
                if line.startswith("DATA:"):
                    print("✅ Dados sendo recebidos")
                    data_received = True
                    break
            time.sleep(0.1)
        
        if not data_received:
            print("❌ Nenhum dado recebido")
        
        ser.close()
        
    except Exception as e:
        print(f"❌ Erro de comunicação: {e}")
```

**Soluções**:
```
✅ Verificar:
1. Baud rate correto (115200)
2. Cabo USB funcional
3. Drivers instalados
4. Porta correta selecionada
5. Nenhum outro programa usando a porta
6. Reset do Arduino antes de conectar
```

#### 5. Performance Degradada

**Sintomas**:
- Latência alta de ISR
- Sistema lento para responder
- Perda de pulsos em alta frequência

**Otimizações**:
```cpp
// 1. ISR mínima
void IRAM_ATTR fastPulseISR() {
    // Apenas incrementar contador, nada mais
    pulseCount++;
    lastPulseTime = micros();
}

// 2. Usar timer hardware de alta precisão
#ifdef ESP32
    hw_timer_t* timer = timerBegin(0, 80, true);
    timerAttachInterrupt(timer, &timerISR, true);
    timerAlarmWrite(timer, 1000000, true); // 1 segundo
    timerAlarmEnable(timer);
#endif

// 3. Otimizar acesso atômico
#if defined(__arm__)
    // ARM tem operações atômicas nativas de 32-bit
    #define ATOMIC_READ(var) (var)
    #define ATOMIC_WRITE(var, val) (var = val)
#else
    // AVR precisa de proteção manual
    #define ATOMIC_READ(var) atomicRead32(var)
    #define ATOMIC_WRITE(var, val) atomicWrite32(var, val)
#endif

// 4. Reduzir operações em ISR
void optimizedTimerISR() {
    // Cache valores locais
    uint32_t pulses = pulseCounter;
    pulseCounter = 0;
    
    // Fazer cálculos fora da ISR
    pendingCalculation = true;
    pendingPulses = pulses;
}
```

### Troubleshooting por Plataforma

#### Arduino Mega Específico

```cpp
// Verificação de conflitos de timer
void checkTimerConflicts() {
    Serial.println("Timer Status Check:");
    Serial.print("Timer1 TCCR1B: 0x"); Serial.println(TCCR1B, HEX);
    Serial.print("Timer3 TCCR3B: 0x"); Serial.println(TCCR3B, HEX);
    Serial.print("Timer4 TCCR4B: 0x"); Serial.println(TCCR4B, HEX);
    Serial.print("Timer5 TCCR5B: 0x"); Serial.println(TCCR5B, HEX);
    
    // Verificar se Servo library está conflitando
    #ifdef SERVO_VERSION
        Serial.println("WARNING: Servo library detected - may conflict with Timer1!");
    #endif
}

// Mapeamento de pinos de interrupção
void checkInterruptPins() {
    Serial.println("Available interrupt pins on Mega:");
    Serial.println("Pin 2  -> INT0 (recommended)");
    Serial.println("Pin 3  -> INT1 (recommended)");
    Serial.println("Pin 18 -> INT5");
    Serial.println("Pin 19 -> INT4");
    Serial.println("Pin 20 -> INT3");
    Serial.println("Pin 21 -> INT2");
}
```

#### ESP32 Específico

```cpp
// Configuração de Core para ISR
void setupESP32MultiCore() {
    // Mover ISR para Core 0 (dedicado)
    xTaskCreatePinnedToCore(
        tachometerTask,   // Função da task
        "TachTask",       // Nome da task
        2048,             // Stack size
        NULL,             // Parâmetros
        1,                // Prioridade
        NULL,             // Handle da task
        0                 // Core 0 (dedicado para ISR)
    );
}

// Verificar configuração de GPIO
void checkESP32GPIO() {
    Serial.println("ESP32 GPIO Configuration:");
    Serial.print("GPIO pin: "); Serial.println(IR_SENSOR_PIN);
    Serial.print("Pin mode: "); Serial.println(gpio_get_direction(IR_SENSOR_PIN));
    Serial.print("Pull-up enabled: "); 
    Serial.println(gpio_pullup_en(IR_SENSOR_PIN) == ESP_OK ? "YES" : "NO");
}

// Problemas comuns ESP32
void esp32Troubleshooting() {
    Serial.println("ESP32 Common Issues:");
    Serial.println("1. WiFi interference - disable WiFi during testing");
    Serial.println("2. Brown-out detector - check power supply stability");
    Serial.println("3. Flash frequency conflicts - use 40MHz flash");
    
    // Desabilitar WiFi para reduzir interferência
    WiFi.mode(WIFI_OFF);
    btStop();
}
```

#### Teensy 4.1 Específico

```cpp
// Configuração de prioridade de interrupção
void setupTeensyPriorities() {
    // Configurar prioridade máxima para timer
    NVIC_SET_PRIORITY(IRQ_PIT, 0);
    
    // Configurar prioridade alta para GPIO
    NVIC_SET_PRIORITY(IRQ_GPIO1, 1);
    
    // Verificar configuração
    Serial.print("Timer priority: "); Serial.println(NVIC_GET_PRIORITY(IRQ_PIT));
    Serial.print("GPIO priority: "); Serial.println(NVIC_GET_PRIORITY(IRQ_GPIO1));
}

// Otimizações específicas Teensy
void teensyOptimizations() {
    // Usar FASTRUN para código crítico
    FASTRUN void criticalTimerISR() {
        // Código da ISR em RAM rápida
    }
    
    // Configurar clock para máxima precisão
    set_arm_clock(600000000); // 600 MHz
}
```

### Ferramentas de Debug

#### Monitor Serial Avançado

```cpp
/**
 * Sistema de debug avançado com múltiplos níveis.
 */
class DebugMonitor {
private:
    enum DebugLevel {
        DEBUG_NONE = 0,
        DEBUG_ERROR = 1,
        DEBUG_WARNING = 2,
        DEBUG_INFO = 3,
        DEBUG_VERBOSE = 4
    };
    
    DebugLevel currentLevel = DEBUG_INFO;
    bool timestampEnabled = true;
    
public:
    void setLevel(DebugLevel level) {
        currentLevel = level;
    }
    
    void error(const char* message) {
        if (currentLevel >= DEBUG_ERROR) {
            printWithTimestamp("ERROR", message);
        }
    }
    
    void warning(const char* message) {
        if (currentLevel >= DEBUG_WARNING) {
            printWithTimestamp("WARN", message);
        }
    }
    
    void info(const char* message) {
        if (currentLevel >= DEBUG_INFO) {
            printWithTimestamp("INFO", message);
        }
    }
    
    void verbose(const char* message) {
        if (currentLevel >= DEBUG_VERBOSE) {
            printWithTimestamp("VERB", message);
        }
    }
    
private:
    void printWithTimestamp(const char* level, const char* message) {
        if (timestampEnabled) {
            Serial.print("[");
            Serial.print(millis());
            Serial.print("] ");
        }
        Serial.print("[");
        Serial.print(level);
        Serial.print("] ");
        Serial.println(message);
    }
};

// Instância global
DebugMonitor debugMonitor;

// Macros para facilitar uso
#define DEBUG_ERROR(msg) debugMonitor.error(msg)
#define DEBUG_WARN(msg) debugMonitor.warning(msg)
#define DEBUG_INFO(msg) debugMonitor.info(msg)
#define DEBUG_VERBOSE(msg) debugMonitor.verbose(msg)
```

#### Profiler de Performance

```cpp
/**
 * Profiler simples para medir performance de ISR.
 */
class SimpleProfiler {
private:
    uint32_t isrStartTime;
    uint32_t maxIsrTime = 0;
    uint32_t totalIsrTime = 0;
    uint32_t isrCallCount = 0;
    
public:
    void startISRTiming() {
        isrStartTime = micros();
    }
    
    void endISRTiming() {
        uint32_t elapsed = micros() - isrStartTime;
        
        totalIsrTime += elapsed;
        isrCallCount++;
        
        if (elapsed > maxIsrTime) {
            maxIsrTime = elapsed;
        }
    }
    
    void printStatistics() {
        if (isrCallCount > 0) {
            Serial.println("=== ISR Performance Statistics ===");
            Serial.print("Total ISR calls: "); Serial.println(isrCallCount);
            Serial.print("Average ISR time: "); 
            Serial.print(totalIsrTime / isrCallCount); Serial.println(" µs");
            Serial.print("Maximum ISR time: "); 
            Serial.print(maxIsrTime); Serial.println(" µs");
            Serial.print("Total ISR overhead: "); 
            Serial.print((totalIsrTime * 100) / (millis() * 1000)); Serial.println("%");
        }
    }
    
    void reset() {
        maxIsrTime = 0;
        totalIsrTime = 0;
        isrCallCount = 0;
    }
};

// Uso no código da ISR
SimpleProfiler profiler;

void optimizedPulseISR() {
    profiler.startISRTiming();
    
    // Código da ISR aqui
    pulseCounter++;
    lastPulseTime = micros();
    
    profiler.endISRTiming();
}
```

---

## Referências

### Documentação Técnica

#### Padrões e Normas

1. **DO-178C** - Software Considerations in Airborne Systems and Equipment Certification
   - Level A-E software development standards
   - Verification and validation requirements
   - Code review and testing protocols

2. **DO-331** - Model-Based Development and Verification Supplement to DO-178C
   - MBSE (Model-Based Systems Engineering) guidelines
   - SysML modeling standards
   - Verification of model-based designs

3. **DO-332** - Object-Oriented Technology and Related Techniques Supplement to DO-178C
   - C++ development guidelines for safety-critical systems
   - Object-oriented design patterns for embedded systems
   - Memory management and exception handling

4. **DO-333** - Formal Methods Supplement to DO-178C
   - Mathematical verification techniques
   - Proof-based validation methods
   - Formal specification languages

5. **RTCA DO-297** - Integrated Modular Avionics (IMA) Development Guidance
   - Multi-core processing guidelines
   - Resource partitioning and isolation
   - Timing analysis for real-time systems

#### Microcontroller Documentation

**Arduino Mega 2560 (ATmega2560)**:
- [ATmega2560 Datasheet](https://www.microchip.com/wwwproducts/en/ATmega2560)
- Timer/Counter configuration and operation
- External interrupt handling
- AVR instruction set and timing

**ESP32**:
- [ESP32 Technical Reference Manual](https://www.espressif.com/sites/default/files/documentation/esp32_technical_reference_manual_en.pdf)
- High-resolution timer (esp_timer) API
- GPIO and interrupt configuration
- Dual-core processing architecture

**Teensy 4.1 (iMXRT1062)**:
- [Teensy 4.1 Documentation](https://www.pjrc.com/teensy/teensy41.html)
- ARM Cortex-M7 performance optimization
- IntervalTimer library documentation
- High-speed GPIO operations

### Algoritmos de Filtragem Digital

#### Livros e Papers

1. **"Digital Signal Processing: Principles, Algorithms, and Applications"** - Proakis & Manolakis
   - Capítulo 7: IIR Filter Design
   - Capítulo 8: FIR Filter Design
   - Moving Average implementations

2. **"Embedded Signal Processing with the Micro Signal Architecture"** - Woon-Seng Gan
   - Real-time filtering on microcontrollers
   - Integer arithmetic optimizations
   - Fixed-point vs floating-point considerations

3. **IEEE Papers**:
   - "Low-pass filtering of noisy signals in embedded systems" - IEEE Trans. Industrial Electronics
   - "Noise reduction techniques for rotational speed measurement" - IEEE Sensors Journal

#### Online Resources

- **DSP Guide**: [www.dspguide.com](http://www.dspguide.com)
- **Microchip Application Notes**: AN699 - Anti-Aliasing, Analog Filters for Data Acquisition Systems
- **Texas Instruments**: SLAA447 - Single-Supply Op Amp Design Techniques

### Implementação e Boas Práticas

#### Embedded Systems

1. **"Making Embedded Systems"** - Elecia White
   - Interrupt handling best practices
   - Real-time system design
   - Debugging embedded systems

2. **"Better Embedded System Software"** - Philip Koopman
   - Code review guidelines
   - Safety-critical software development
   - Testing strategies for embedded systems

3. **"Real-Time Systems Design and Analysis"** - Phillip A. Laplante
   - Timing analysis and verification
   - Concurrent programming techniques
   - Priority assignment algorithms

#### C++ for Embedded

1. **"Effective Modern C++"** - Scott Meyers
   - C++11/14/17 features for embedded
   - Smart pointers and RAII
   - Move semantics and performance

2. **"Embedded Programming with Modern C++ Cookbook"** - Igor Viarheichyk
   - Template metaprogramming for embedded
   - Compile-time optimizations
   - Memory-constrained programming

### Ferramentas de Desenvolvimento

#### IDEs e Toolchains

- **PlatformIO**: [platformio.org](https://platformio.org)
- **Arduino IDE**: [arduino.cc](https://www.arduino.cc)
- **Teensyduino**: [pjrc.com/teensy/teensyduino.html](https://www.pjrc.com/teensy/teensyduino.html)
- **ESP-IDF**: [docs.espressif.com/projects/esp-idf](https://docs.espressif.com/projects/esp-idf)

#### Testing and Analysis Tools

- **Logic Analyzers**: Saleae Logic, Rigol MSO series
- **Oscilloscopes**: Keysight DSOX series, Tektronix MSO series
- **Static Analysis**: PC-lint Plus, Clang Static Analyzer
- **Code Coverage**: gcov, LDRA Testbed

---

## Glossário

### Termos Técnicos

**CTC (Clear Timer on Compare)**
: Modo de operação de timer onde o contador é resetado automaticamente quando atinge o valor de comparação, gerando interrupção periódica precisa.

**Debounce**
: Técnica para eliminar múltiplas transições causadas por ruído mecânico ou elétrico em sinais digitais. Implementado por tempo mínimo entre pulsos válidos.

**ISR (Interrupt Service Routine)**
: Função especial executada automaticamente quando ocorre uma interrupção de hardware. Deve ser rápida e minimizar operações complexas.

**Atomic Operation**
: Operação que é executada completamente sem possibilidade de interrupção, garantindo consistência de dados entre threads ou ISRs.

**Low-Pass Filter**
: Filtro digital que permite passar frequências baixas e atenua frequências altas, usado para reduzir ruído em medições.

**Moving Average**
: Técnica de filtragem que calcula a média de N amostras anteriores, proporcionando suavização temporal dos dados.

**Prescaler**
: Divisor de frequência usado em timers para reduzir a frequência do clock principal, permitindo períodos maiores de contagem.

**Thread-Safe**
: Propriedade de código que pode ser executado simultaneamente por múltiplas threads sem corrupção de dados.

### Acronimos e Abreviações

**ADC**: Analog-to-Digital Converter
**API**: Application Programming Interface
**ARM**: Advanced RISC Machine
**AVR**: Alf and Vegard's RISC processor
**BPS**: Bits Per Second (taxa de transmissão serial)
**CPU**: Central Processing Unit
**CRC**: Cyclic Redundancy Check
**DMA**: Direct Memory Access
**DSP**: Digital Signal Processing
**EMI**: Electromagnetic Interference
**FIFO**: First In, First Out
**GPIO**: General Purpose Input/Output
**GUI**: Graphical User Interface
**Hz**: Hertz (unidade de frequência)
**I2C**: Inter-Integrated Circuit
**IDE**: Integrated Development Environment
**IIR**: Infinite Impulse Response
**MCU**: Microcontroller Unit
**MIPS**: Million Instructions Per Second
**NVIC**: Nested Vector Interrupt Controller
**OOP**: Object-Oriented Programming
**PCB**: Printed Circuit Board
**PWM**: Pulse Width Modulation
**RAM**: Random Access Memory
**RISC**: Reduced Instruction Set Computer
**ROM**: Read-Only Memory
**RPM**: Revolutions Per Minute
**RTC**: Real-Time Clock
**RTOS**: Real-Time Operating System
**SPI**: Serial Peripheral Interface
**SRAM**: Static Random Access Memory
**UART**: Universal Asynchronous Receiver-Transmitter
**USB**: Universal Serial Bus
**µs**: Microsecond (microssegundo)
**ms**: Millisecond (milissegundo)

### Unidades e Medidas

**Frequência**
- Hz (Hertz): Ciclos por segundo
- kHz (Kilohertz): 1.000 Hz
- MHz (Megahertz): 1.000.000 Hz

**Tempo**
- µs (Microssegundo): 0,000001 segundo
- ms (Milissegundo): 0,001 segundo
- s (Segundo): unidade base de tempo

**Velocidade Angular**
- RPM: Revoluções por minuto
- RPS: Revoluções por segundo
- rad/s: Radianos por segundo

**Memória**
- Byte: 8 bits
- KB (Kilobyte): 1.024 bytes
- MB (Megabyte): 1.024 KB

### Conceitos de Sistemas Embarcados

**Boot Loader**
: Programa responsável por inicializar o sistema e carregar o firmware principal na memória.

**Cross-Compilation**
: Processo de compilar código em uma arquitetura para execução em arquitetura diferente (ex: PC compilando para ARM).

**Firmware**
: Software de baixo nível gravado permanentemente no microcontrolador, responsável pelo controle direto do hardware.

**Hardware Abstraction Layer (HAL)**
: Camada de software que fornece interface uniforme para acessar diferentes componentes de hardware.

**Memory Mapping**
: Técnica onde registradores de hardware são acessados como endereços de memória específicos.

**Peripheral**
: Componente de hardware conectado ao microcontrolador (sensores, atuadores, interfaces de comunicação).

**Register**
: Localização de memória especial usada para configurar e controlar periféricos do microcontrolador.

**Watchdog Timer**
: Timer de segurança que reseta o sistema se não for "alimentado" periodicamente, detectando travamentos.

---

## Conclusão

Este documento apresentou um sistema completo de tacômetro industrial implementado seguindo rigorosamente os padrões DO-178C para desenvolvimento de software crítico. O sistema demonstra:

### Principais Realizações

1. **Arquitetura Robusta**: Implementação de dupla interrupção com operações thread-safe e filtragem digital avançada

2. **Multi-Plataforma**: Compatibilidade total com Arduino Mega, ESP32 e Teensy 4.1, com otimizações específicas para cada arquitetura

3. **Interface Completa**: Sistema GUI Python integrado para monitoramento e controle em tempo real

4. **Validação Rigorosa**: Suite completa de testes automatizados e ferramentas de diagnóstico

5. **Documentação Extensiva**: Cobertura completa desde teoria de operação até troubleshooting avançado

### Contribuições Técnicas

- **Operações Apenas com Inteiros**: Eliminação de operações de ponto flutuante para máxima performance
- **Filtragem Digital Otimizada**: Implementação de low-pass filter e moving average em aritmética inteira
- **Protocolo Serial Robusto**: Sistema de comunicação com verificação de erros e recuperação automática
- **Padrões de Código DO-178C**: Estrutura compliant com desenvolvimento aeroespacial crítico

### Aplicações Recomendadas

Este sistema é adequado para:
- **Controle Industrial**: Monitoramento de motores, bombas e turbinas
- **Aeroespacial**: Sistemas críticos de propulsão e controle
- **Automotivo**: Monitoramento de RPM de motores e transmissões
- **Pesquisa Científica**: Bancadas de teste e validação experimental
- **Energia Renovável**: Controle de turbinas eólicas e sistemas rotativos

### Trabalhos Futuros

Possíveis extensões do sistema incluem:
- **Comunicação Wireless**: Integração WiFi/Bluetooth para ESP32
- **Múltiplos Sensores**: Suporte para encoders quadratura e sensores Hall
- **Análise Espectral**: FFT para análise de vibrações e harmônicos
- **Integração IoT**: Conectividade com plataformas cloud e SCADA
- **Safety Certification**: Extensão para certificação SIL e outras normas

O sistema apresentado estabelece uma base sólida para desenvolvimento de instrumentação industrial de alta precisão, seguindo as melhores práticas de engenharia de software e compatível com os mais rigorosos padrões de qualidade da indústria.

---

**© 2012 Omar Achraf - Todos os direitos reservados**  
**Email**: omarachraf@gmail.com    |    [www.uvsbr.com.br](www.uvsbr.com.br) 
