#pragma once

#include <Arduino.h> // garante definição de uint8_t

// Mapeamento de pinos unificado (GPIO lógico)
#if defined(ESP8266)
// Relay original
constexpr uint8_t RELAY_PIN = 2; // D4

// Display TM1637
constexpr uint8_t TM1637_DIO = 0; // D3
constexpr uint8_t TM1637_CLK = 4; // D2

// Módulo 74HC595 (Shift Register)
constexpr uint8_t HC595_DATA_PIN = 4;  // D2 - QH/SER/DS (Dados)
constexpr uint8_t HC595_LATCH_PIN = 0; // D3 - RCLK/LATCH (Travar saída)
constexpr uint8_t HC595_CLOCK_PIN = 2; // D4 - SCLK/SRCLK (Clock)

#elif defined(ESP32)
// Relay original
constexpr uint8_t RELAY_PIN = 2;

// Display TM1637
constexpr uint8_t TM1637_DIO = 0;
constexpr uint8_t TM1637_CLK = 4;

// Módulo 74HC595 (Shift Register)
constexpr uint8_t HC595_DATA_PIN = 4;
constexpr uint8_t HC595_LATCH_PIN = 0;
constexpr uint8_t HC595_CLOCK_PIN = 2;

#else
#error "Plataforma não suportada"
#endif
