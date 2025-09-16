#pragma once

#include <Arduino.h> // garante definição de uint8_t

// Mapeamento de pinos unificado (GPIO lógico)
#if defined(ESP8266)
constexpr uint8_t RELAY_PIN = 2;  // D4
constexpr uint8_t TM1637_DIO = 0; // D3
constexpr uint8_t TM1637_CLK = 4; // D2
#elif defined(ESP32)
constexpr uint8_t RELAY_PIN = 2;
constexpr uint8_t TM1637_DIO = 0;
constexpr uint8_t TM1637_CLK = 4;
#else
#error "Plataforma não suportada"
#endif
