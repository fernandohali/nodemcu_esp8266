# Módulo 74HC595 - Expansor de Saídas Digitais

## 📋 Visão Geral

O módulo 74HC595 permite expandir as saídas digitais do ESP8266 de 3 pinos para 8 saídas controláveis, usando apenas 3 pinos de controle. Ideal para controlar LEDs, relays, motores, ou qualquer dispositivo que precise de sinal digital.

## 🔌 Conexões

| Módulo (74HC595)  | Função                  | NodeMCU (pino físico) | GPIO real |
| ----------------- | ----------------------- | --------------------- | --------- |
| **QH / SER / DS** | Dados de entrada (DATA) | D2                    | GPIO4     |
| **RCLK / LATCH**  | Travar saída            | D3                    | GPIO0     |
| **SCLK / SRCLK**  | Clock de deslocamento   | D4                    | GPIO2     |
| **VCC**           | Alimentação             | 3V3 ou 5V             | –         |
| **GND**           | Terra                   | GND                   | –         |

## 💾 Saídas Disponíveis

O 74HC595 oferece 8 saídas digitais:

- **Q0** - Saída 0 (LSB)
- **Q1** - Saída 1
- **Q2** - Saída 2
- **Q3** - Saída 3
- **Q4** - Saída 4
- **Q5** - Saída 5
- **Q6** - Saída 6
- **Q7** - Saída 7 (MSB)

## 🎮 Comandos WebSocket

### Controle Individual de Pinos

```bash
# Ligar pino específico (0-7)
curl -X POST http://localhost:8081/api/ws/connections/CAR-1758112436133-q5wqs3t1v \
  -H "Content-Type: application/json" \
  -d '{"action": "hc595_pin_0_on"}'

# Desligar pino específico (0-7)
curl -X POST http://localhost:8081/api/ws/connections/CAR-1758112436133-q5wqs3t1v \
  -H "Content-Type: application/json" \
  -d '{"action": "hc595_pin_7_off"}'
```

### Controle de Todos os Pinos

```bash
# Ligar todas as saídas
curl -X POST http://localhost:8081/api/ws/connections/CAR-1758112436133-q5wqs3t1v \
  -H "Content-Type: application/json" \
  -d '{"action": "hc595_all_on"}'

# Desligar todas as saídas
curl -X POST http://localhost:8081/api/ws/connections/CAR-1758112436133-q5wqs3t1v \
  -H "Content-Type: application/json" \
  -d '{"action": "hc595_all_off"}'
```

### Controle por Byte (8 bits)

```bash
# Definir padrão específico (decimal)
# 170 = 0b10101010 = Q7,Q5,Q3,Q1 ligados
curl -X POST http://localhost:8081/api/ws/connections/CAR-1758112436133-q5wqs3t1v \
  -H "Content-Type: application/json" \
  -d '{"action": "hc595_byte_170"}'

# 85 = 0b01010101 = Q6,Q4,Q2,Q0 ligados
curl -X POST http://localhost:8081/api/ws/connections/CAR-1758112436133-q5wqs3t1v \
  -H "Content-Type: application/json" \
  -d '{"action": "hc595_byte_85"}'
```

### Efeitos Visuais

```bash
# Running light (LED correndo)
curl -X POST http://localhost:8081/api/ws/connections/CAR-1758112436133-q5wqs3t1v \
  -H "Content-Type: application/json" \
  -d '{"action": "hc595_running_light"}'
```

## 💡 Exemplos Práticos

### 1. Controlar 8 LEDs

Conecte LEDs (com resistores) nas saídas Q0-Q7:

```bash
# Piscar LED 0
curl -X POST ... -d '{"action": "hc595_pin_0_on"}'
sleep 1
curl -X POST ... -d '{"action": "hc595_pin_0_off"}'

# Padrão alternado
curl -X POST ... -d '{"action": "hc595_byte_170"}'  # 10101010
```

### 2. Controlar 8 Relays

Conecte módulos relay nas saídas:

```bash
# Ligar relays 0, 2, 4, 6
curl -X POST ... -d '{"action": "hc595_byte_85"}'   # 01010101
```

### 3. Display de 7 Segmentos

Use 7 saídas para controlar um display:

```bash
# Número "8" (todos os segmentos)
curl -X POST ... -d '{"action": "hc595_byte_127"}'  # 01111111
```

## 🛠️ Funções em Código

```cpp
// Inicializar módulo
HC595::begin();

// Controle individual
HC595::setPin(0, HIGH);     // Liga Q0
HC595::setPin(7, LOW);      // Desliga Q7

// Controle por byte
HC595::setByte(0b10101010); // Define padrão

// Aplicar mudanças
HC595::update();            // IMPORTANTE: sempre chamar após mudanças

// Ler estado
bool estado = HC595::getPin(3);     // Lê Q3
uint8_t todos = HC595::getByte();   // Lê todos os pinos

// Efeitos
HC595::allOn();                     // Liga tudo
HC595::allOff();                    // Desliga tudo
HC595::runningLight(200);           // Efeito corrida
HC595::blinkPin(5, 500);           // Pisca Q5
```

## 📊 Conversão Decimal ↔ Binário

| Decimal | Binário  | Saídas Ligadas |
| ------- | -------- | -------------- |
| 0       | 00000000 | Nenhuma        |
| 1       | 00000001 | Q0             |
| 3       | 00000011 | Q0, Q1         |
| 15      | 00001111 | Q0, Q1, Q2, Q3 |
| 85      | 01010101 | Q0, Q2, Q4, Q6 |
| 170     | 10101010 | Q1, Q3, Q5, Q7 |
| 255     | 11111111 | Todas          |

## 🔧 Troubleshooting

### Problema: Saídas não respondem

- Verifique alimentação VCC (3.3V ou 5V)
- Confirme conexões GND
- Teste continuidade dos fios

### Problema: Algumas saídas ficam ligadas

- Execute `hc595_all_off` para resetar
- Verifique se `HC595::update()` está sendo chamado

### Problema: Efeitos não funcionam

- Confirme que o WebSocket está conectado
- Verifique logs no Serial Monitor
- Teste comandos individuais primeiro

## ⚡ Limitações

- **Corrente máxima por saída**: 35mA
- **Corrente total**: 70mA (todos os pinos)
- **Tensão de saída**: VCC (3.3V ou 5V)
- **Para cargas maiores**: Use módulos relay ou transistores
