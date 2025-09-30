/**
 * @file main.cpp
 * @brief Sistema de controle para ESP8266 NodeMCU
 * @author Fernando Hali
 * @version 2.0
 *
 * Sistema refatorado para controle de operações com WebSocket,
 * display TM1637, relay e módulo 74HC595.
 */

// ===== INCLUDES PRINCIPAIS =====
#include "pins.h"
#include "Config/config.h"
#include "System/system_info.h"
#include "Operation/operation_manager.h"
#include "WebSocket/websocket_manager.h"
#include "Commands/serial_commands.h"
#include "Wifi/wifi.h"
#include "Display/Display.h"
#include "Reley/reley.h"
#include "HC595/HC595.h"
#include "Status/status_led.h"

// ===== VALIDAÇÃO DE CONFIGURAÇÃO =====
#ifdef STATIC_IP_ADDR
#ifndef STATIC_IP_GW
#error "Defina também STATIC_IP_GW quando usar STATIC_IP_ADDR"
#endif
#ifndef STATIC_IP_MASK
#error "Defina também STATIC_IP_MASK quando usar STATIC_IP_ADDR"
#endif
#endif

// ===== Modo de compatibilidade do HELLO =====
#define WS_HELLO_SIMPLE
#undef WS_HELLO_COMPAT

/**
 * @brief Executa teste inicial do módulo HC595
 */
void performHC595Test()
{
  Serial.println(F("[HC595] Executando teste inicial..."));
  HC595::runningLight(100);
  delay(500);
  HC595::setByte(0b10101010);
  HC595::update();
  delay(1000);
  HC595::allOff();
  Serial.println(F("[HC595] Teste concluído"));
}

/**
 * @brief Configura IP estático se definido
 */
void configureStaticIP()
{
#ifdef STATIC_IP_ADDR
  Net::configureStaticIp(
      STATIC_IP_ADDR,
      STATIC_IP_GW,
      STATIC_IP_MASK,
#ifdef STATIC_IP_DNS1
      STATIC_IP_DNS1,
#else
      nullptr,
#endif
#ifdef STATIC_IP_DNS2
      STATIC_IP_DNS2
#else
      nullptr
#endif
  );
#endif
}

// ===== FUNÇÃO SETUP =====

/**
 * @brief Inicialização do sistema
 */
void setup()
{
  Serial.begin(115200);

#ifdef ESP8266
  Serial.setDebugOutput(false); // Evita logs extras do WiFi/SDK
#endif

  Serial.println(F("\n===== INICIANDO SISTEMA ESP8266 ====="));

  // ===== INICIALIZAÇÃO DOS MÓDULOS =====
  StatusLED::begin();
  Relay::begin();
  Disp::begin();
  HC595::begin();

  // Teste inicial do HC595
  performHC595Test();

  // Inicializar gerenciadores
  Operation::initialize();
  WebSocketManager::initialize();

  // ===== CONFIGURAÇÃO DE REDE =====
  Net::begin(WIFI_SSID, WIFI_PASSWORD);
  configureStaticIP();

  Serial.println(F("Conectando ao WiFi..."));
  if (!Net::waitConnected(15000))
  {
    Serial.println(F("Falha ao conectar no WiFi dentro do timeout."));
  }
  else
  {
    SystemInfo::showSystemInfo();
  }

  Net::setupTime();

  // ===== CONFIGURAÇÃO DO WEBSOCKET =====
  if (strlen(WS_HOST_STR) == 0)
  {
    Serial.println(F("[WS][ERRO] WS_HOST_STR não definido. Ajuste build_flags em platformio.ini"));
  }
  else
  {
    WebSocketManager::startConnection();
  }

  // Status inicial
  Disp::showStatus("STOPPED");

  Serial.println(F("===== SISTEMA INICIALIZADO ====="));
  Serial.printf("Tempo de inicialização: %lu ms\n", millis());
  Serial.println();
}

/**
 * @brief Loop principal do sistema
 */
void loop()
{
  // ===== STATUS PERIÓDICO DO SISTEMA =====
  static unsigned long lastSystemStatus = 0;
  if (millis() - lastSystemStatus > 60000) // A cada 1 minuto
  {
    lastSystemStatus = millis();
    Serial.println(F("+===========================================+"));
    Serial.println(F("| STATUS DO SISTEMA                     |"));
    Serial.println(F("+-------------------------------------------+"));
    Serial.printf("| Uptime     : %lu ms (%.1f min)       |\n", millis(), millis() / 60000.0);
    Serial.printf("| WiFi       : %s                 |\n", WiFi.isConnected() ? "Conectado  " : "Desconectado");
    Serial.printf("| RAM livre  : %d bytes            |\n", ESP.getFreeHeap());
    Serial.printf("| Operacao   : %s                     |\n", Operation::getStatusString());
    Serial.println(F("+===========================================+"));
    Serial.println();
  }

  // ===== ATUALIZAÇÃO DOS MÓDULOS =====
  Net::loop();
  StatusLED::update(); // Atualiza LED WiFi baseado no status da conexão
  WebSocketManager::update();
  Operation::update();
  Disp::loop();

  // ===== PROCESSAMENTO DE COMANDOS SERIAL =====
  SerialCommands::processCommands();

  // ===== DELAY DO LOOP =====
  delay(LOOP_DELAY_MS);
}
