#pragma once

#include <Arduino.h>

/**
 * Módulo para controle do 74HC595 (Shift Register)
 *
 * O 74HC595 permite expandir as saídas digitais do ESP8266.
 * Com um chip você obtém 8 saídas digitais usando apenas 3 pinos do ESP.
 *
 * Conexões:
 * - HC595_DATA_PIN (D2/GPIO4)  -> QH/SER/DS (pino 14)
 * - HC595_LATCH_PIN (D3/GPIO0) -> RCLK/LATCH (pino 12)
 * - HC595_CLOCK_PIN (D4/GPIO2) -> SCLK/SRCLK (pino 11)
 * - VCC -> 3V3 ou 5V
 * - GND -> GND
 *
 * Uso:
 *   HC595::begin();
 *   HC595::setPin(0, HIGH);     // Liga saída Q0
 *   HC595::setPin(7, LOW);      // Desliga saída Q7
 *   HC595::setByte(0b10101010); // Define todas as 8 saídas
 *   HC595::update();            // Aplica as mudanças
 */

namespace HC595
{

    // Inicializa o módulo 74HC595
    void begin();

    // Define o estado de um pino específico (0-7)
    void setPin(uint8_t pin, bool state);

    // Define todos os 8 pinos de uma vez (bit 0 = Q0, bit 7 = Q7)
    void setByte(uint8_t value);

    // Aplica as mudanças nas saídas (shift out + latch)
    void update();

    // Obtém o estado atual de um pino (0-7)
    bool getPin(uint8_t pin);

    // Obtém o valor atual de todos os pinos
    uint8_t getByte();

    // Liga todos os pinos
    void allOn();

    // Desliga todos os pinos
    void allOff();

    // Pisca um pino específico
    void blinkPin(uint8_t pin, uint16_t delayMs = 500);

    // Efeito sequencial (como um LED running)
    void runningLight(uint16_t delayMs = 200);
}