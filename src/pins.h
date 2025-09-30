#pragma once

#include <Arduino.h> // garante definição de uint8_t

// Mapeamento de pinos unificado (GPIO lógico)
#if defined(ESP8266)
// Relay original
constexpr uint8_t RELAY_PIN = 15; // D8

// LED indicador WiFi
constexpr uint8_t WIFI_LED_PIN = 5; // D1 - LED que indica conexão WiFi

// Módulo Display 74HC595 (4 dígitos 7 segmentos)
constexpr uint8_t HC595_DATA_PIN = 13;  // D7 - Dados (DIO)
constexpr uint8_t HC595_LATCH_PIN = 12; // D6 - Latch (RCLK)
constexpr uint8_t HC595_CLOCK_PIN = 14; // D5 - Clock (SCLK)

#elif defined(ESP32)
// Relay original
constexpr uint8_t RELAY_PIN = 2;

// Módulo Display 74HC595 (4 dígitos 7 segmentos)
constexpr uint8_t HC595_DATA_PIN = 13;  // Dados (DIO)
constexpr uint8_t HC595_LATCH_PIN = 12; // Latch (RCLK)
constexpr uint8_t HC595_CLOCK_PIN = 14; // Clock (SCLK)

#else
#error "Plataforma não suportada"
#endif
