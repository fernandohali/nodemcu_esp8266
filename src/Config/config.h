#pragma once

#include <cstddef>
#include <cstdint>

// ===== CONFIGURAÇÕES DE REDE =====
#ifndef WS_HOST_STR
#define WS_HOST_STR ""
#endif

#ifndef CAR_ID_STR
#define CAR_ID_STR "CAR-UNDEFINED"
#endif

// Credenciais WiFi - use as suas próprias credenciais
#ifndef WIFI_SSID
#define WIFI_SSID "WiFi C1N24A"
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "35250509"
#endif

static const uint16_t WS_PORT = 8081;

// ===== CONFIGURAÇÕES DE TIMING =====
#ifndef LOOP_DELAY_MS
#define LOOP_DELAY_MS 50
#endif

#ifndef HEARTBEAT_MS
#define HEARTBEAT_MS 5000 // 5 segundos - mantém servidor sempre informado do status
#endif

#ifndef WS_BASE_RETRY_MS
#define WS_BASE_RETRY_MS 3000
#endif

#ifndef WS_MAX_RETRY_MS
#define WS_MAX_RETRY_MS 30000
#endif

#ifndef WS_HANDSHAKE_TIMEOUT_MS
#define WS_HANDSHAKE_TIMEOUT_MS 8000
#endif

#ifndef WS_HELLO_DELAY_MS
#define WS_HELLO_DELAY_MS 5000
#endif

// ===== CONFIGURAÇÕES DE LOG =====
#ifndef LOG_VERBOSE
#define LOG_VERBOSE 1
#endif

#ifndef LOG_HEARTBEAT_JSON
#define LOG_HEARTBEAT_JSON 0
#endif

#ifndef LOG_STATUS_JSON
#define LOG_STATUS_JSON 0
#endif

#ifndef LOG_COLOR
#define LOG_COLOR 0
#endif

// ===== CONFIGURAÇÕES DE CONEXÃO =====
#ifndef WS_CONNECT_ONCE
#define WS_CONNECT_ONCE 0
#endif

#ifndef WS_DISABLE_HEARTBEAT
#define WS_DISABLE_HEARTBEAT 0 // 0 = heartbeat habilitado, 1 = desabilitado
#endif

// ===== CORES PARA LOG =====
#if LOG_COLOR
#define C_GREEN "\x1b[32m"
#define C_YELLOW "\x1b[33m"
#define C_RED "\x1b[31m"
#define C_CYAN "\x1b[36m"
#define C_RESET "\x1b[0m"
#else
#define C_GREEN ""
#define C_YELLOW ""
#define C_RED ""
#define C_CYAN ""
#define C_RESET ""
#endif

// ===== HOSTS ALTERNATIVOS =====
#if defined(WS_DISABLE_FALLBACK) || (WS_CONNECT_ONCE == 1)
static const char *ALT_WS_HOSTS[] = {WS_HOST_STR};
#else
static const char *ALT_WS_HOSTS[] = {
    WS_HOST_STR,     // IP Ethernet principal: 10.8.113.82
    "192.168.1.114", // IP WiFi do PC
    "localhost",     // Para teste local
    "127.0.0.1",     // Loopback local
    "192.168.1.100", // IPs comuns da rede WiFi
    "192.168.0.100", // IPs comuns de outras redes
    "10.0.0.100"     // IPs comuns de rede corporativa
};
#endif

static const size_t ALT_WS_HOSTS_COUNT = sizeof(ALT_WS_HOSTS) / sizeof(ALT_WS_HOSTS[0]);