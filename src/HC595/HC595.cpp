#include "HC595.h"
#include "../pins.h"

namespace HC595
{

    // Estado atual dos 8 pinos (bit 0 = Q0, bit 7 = Q7)
    static uint8_t currentState = 0;

    void begin()
    {
        // Configura pinos como saída
        pinMode(HC595_DATA_PIN, OUTPUT);
        pinMode(HC595_LATCH_PIN, OUTPUT);
        pinMode(HC595_CLOCK_PIN, OUTPUT);

        // Estado inicial: todos desligados
        digitalWrite(HC595_DATA_PIN, LOW);
        digitalWrite(HC595_LATCH_PIN, LOW);
        digitalWrite(HC595_CLOCK_PIN, LOW);

        currentState = 0;
        update(); // Aplica estado inicial

        Serial.println(F("[HC595] Inicializado - 8 saídas disponíveis"));
    }

    void setPin(uint8_t pin, bool state)
    {
        if (pin > 7)
            return; // Pin inválido

        if (state)
        {
            currentState |= (1 << pin); // Liga o bit
        }
        else
        {
            currentState &= ~(1 << pin); // Desliga o bit
        }
    }

    void setByte(uint8_t value)
    {
        currentState = value;
    }

    void update()
    {
        // Puxa LATCH para baixo para começar a transmissão
        digitalWrite(HC595_LATCH_PIN, LOW);

        // Envia os 8 bits, começando pelo mais significativo (Q7 primeiro)
        for (int8_t i = 7; i >= 0; i--)
        {
            digitalWrite(HC595_DATA_PIN, (currentState >> i) & 1);

            // Pulso de clock para registrar o bit
            digitalWrite(HC595_CLOCK_PIN, HIGH);
            delayMicroseconds(1);
            digitalWrite(HC595_CLOCK_PIN, LOW);
            delayMicroseconds(1);
        }

        // Puxa LATCH para cima para aplicar os dados nas saídas
        digitalWrite(HC595_LATCH_PIN, HIGH);
        delayMicroseconds(1);
        digitalWrite(HC595_LATCH_PIN, LOW);
    }

    bool getPin(uint8_t pin)
    {
        if (pin > 7)
            return false;
        return (currentState >> pin) & 1;
    }

    uint8_t getByte()
    {
        return currentState;
    }

    void allOn()
    {
        setByte(0xFF); // Todos os bits ligados
        update();
    }

    void allOff()
    {
        setByte(0x00); // Todos os bits desligados
        update();
    }

    void blinkPin(uint8_t pin, uint16_t delayMs)
    {
        if (pin > 7)
            return;

        bool originalState = getPin(pin);

        setPin(pin, !originalState);
        update();
        delay(delayMs);

        setPin(pin, originalState);
        update();
    }

    void runningLight(uint16_t delayMs)
    {
        for (uint8_t i = 0; i < 8; i++)
        {
            setByte(1 << i); // Liga apenas o LED atual
            update();
            delay(delayMs);
        }

        // Volta ao estado anterior
        allOff();
    }
}