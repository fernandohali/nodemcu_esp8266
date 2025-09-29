// Novo main refatorado usando módulos
#include "pins.h"
#include "Wifi/wifi.h"
#include "Display/Display.h"
#include "Reley/reley.h"
#include "HC595/HC595.h"
#include <WebSocketsClient.h>
#include <ESP8266HTTPClient.h>
#include "WS/WSUtils.h"

#ifndef LOOP_DELAY_MS
#define LOOP_DELAY_MS 50 // Reduzido para dar mais tempo de processamento
#endif

#ifndef HEARTBEAT_MS
#define HEARTBEAT_MS 10000 // 10s: ajuste com -DHEARTBEAT_MS=5000 p/ testes
#endif

// ===== Configuração de Log (pode ser controlado via build_flags) =====
#ifndef LOG_VERBOSE
#define LOG_VERBOSE 1 // 1 = mostra logs extras, 0 = mínimo
#endif
#ifndef LOG_HEARTBEAT_JSON
#define LOG_HEARTBEAT_JSON 0 // 1 = imprime JSON de heartbeat além da linha humana
#endif
#ifndef LOG_STATUS_JSON
#define LOG_STATUS_JSON 0 // 1 = imprime JSON bruto de status
#endif
#ifndef LOG_COLOR
#define LOG_COLOR 0 // 1 = usa ANSI colors (útil se monitor suporta)
#endif

// ===== Modo de conexão única (não reconecta automaticamente) =====
// Temporariamente desabilitado para debug de estabilidade
#ifndef WS_CONNECT_ONCE
#define WS_CONNECT_ONCE 0 // 0 = permite reconexão, 1 = conexão única
#endif

// ===== Modos de compatibilidade de HELLO =====
// Forçando modo simples para debug de estabilidade
#define WS_HELLO_SIMPLE
#undef WS_HELLO_COMPAT

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

#ifdef STATIC_IP_ADDR
#ifndef STATIC_IP_GW
#error "Defina também STATIC_IP_GW quando usar STATIC_IP_ADDR"
#endif
#ifndef STATIC_IP_MASK
#error "Defina também STATIC_IP_MASK quando usar STATIC_IP_ADDR"
#endif
#endif

static const char *ssid = "WiFi C1N24A";  // TODO: mover para secrets customizado
static const char *password = "35250509"; // TODO: idem acima
#ifndef WS_HOST_STR                       // definido em build_flags
#define WS_HOST_STR ""                    // se vazio => log de aviso e não conecta
#endif
#ifndef CAR_ID_STR
#define CAR_ID_STR "CAR-UNDEFINED" // fallback se não definido
#endif
static const uint16_t WS_PORT = 8081;
static const char *CAR_ID = CAR_ID_STR; // usar macro configurável

// ===== ESTADOS DA OPERAÇÃO =====
enum OperationStatus
{
  OP_STOPPED = 0,    // Parado - relay desligado
  OP_ACTIVE,         // Ativo - contando regressivamente
  OP_PAUSED,         // Pausado - relay ligado, contador congelado
  OP_LIBERATED_TIME, // Liberado com tempo - conta normalmente
  OP_LIBERATED_FREE  // Liberado sem tempo - relay ligado, sem contador
};

// ===== VARIÁVEIS DE CONTROLE DE OPERAÇÃO =====
static OperationStatus g_operationStatus = OP_STOPPED;
static String g_sessionCarId = "";          // carId da sessão atual
static int g_initialMinutes = 0;            // minutos iniciais recebidos
static int g_remainingSeconds = 0;          // segundos restantes (contagem regressiva)
static int g_extraSeconds = 0;              // segundos extras (contagem progressiva após 0:00)
static bool g_isCountingDown = true;        // true = regressiva, false = progressiva
static unsigned long g_lastCountUpdate = 0; // último update do contador
static bool g_relayState = false;           // estado atual do relay

WebSocketsClient webSocket;
static unsigned long lastHeartbeatAt = 0;
static String g_lastStatus = "STOPPED";
static unsigned long lastInboundAt = 0;     // último evento recebido do servidor
static unsigned long lastHealthcheckAt = 0; // último healthcheck HTTP
static unsigned long lastWsConnectAttemptAt = 0;
static uint32_t wsConnectAttempts = 0;
static bool wsConnectGaveUp = false; // Para WS_CONNECT_ONCE: true após primeira tentativa falhada
// Métricas de sessão WebSocket
static unsigned long currentSessionStartedAt = 0; // millis() no CONNECTED
static uint32_t sessionSentFrames = 0;            // Contagem de frames enviados
static uint32_t sessionRecvFrames = 0;            // Contagem de frames recebidos (TEXT, PING, PONG)
// Controle de reconexão (backoff manual)
static unsigned long wsNextAllowedConnectAt = 0;
static bool wsInHandshake = false;
static unsigned long wsHandshakeStartedAt = 0;

#ifndef WS_BASE_RETRY_MS
#define WS_BASE_RETRY_MS 3000
#endif
#ifndef WS_MAX_RETRY_MS
#define WS_MAX_RETRY_MS 30000
#endif
#ifndef WS_HANDSHAKE_TIMEOUT_MS
#define WS_HANDSHAKE_TIMEOUT_MS 8000
#endif

// Opção de diagnóstico: atrasar envio do hello/status após conectar para testar se envio imediato causa queda
#ifndef WS_HELLO_DELAY_MS
#define WS_HELLO_DELAY_MS 5000 // 5s delay para estabilizar conexão
#endif
static bool pendingHelloSend = false;
static unsigned long pendingHelloScheduledAt = 0;
#if defined(WS_DISABLE_FALLBACK) || (WS_CONNECT_ONCE == 1)
static const char *ALT_WS_HOSTS[] = {
    WS_HOST_STR};
#else
static const char *ALT_WS_HOSTS[] = {
    WS_HOST_STR,
    "192.168.0.100",
    "192.168.0.101",
    // "192.168.0.102", // Removido para evitar colisão com IP da própria placa
    "192.168.0.103"}; // ajuste conforme rede
#endif
static const size_t ALT_WS_HOSTS_COUNT = sizeof(ALT_WS_HOSTS) / sizeof(ALT_WS_HOSTS[0]);
static size_t currentHostIndex = 0;

void startWebSocket();

// Função utilitária para heap livre
static inline uint32_t getFreeHeap()
{
#if defined(ESP8266) || defined(ESP32)
  return ESP.getFreeHeap();
#else
  return 0;
#endif
}

// Mostra informações completas do sistema na inicialização
void showSystemInfo()
{
  Serial.println(F("\n======= INFORMAÇÕES DO SISTEMA ======="));

  // Informações WiFi
  Serial.println(F("--- WIFI ---"));
  Serial.print(F("IP: "));
  Serial.println(Net::ip());
  Serial.print(F("RSSI: "));
  Serial.print(Net::rssi());
  Serial.println(F(" dBm"));
  Serial.print(F("MAC: "));
  Serial.println(WiFi.macAddress());
  Serial.print(F("Gateway: "));
  Serial.println(WiFi.gatewayIP());
  Serial.print(F("Subnet: "));
  Serial.println(WiFi.subnetMask());
  Serial.print(F("DNS1: "));
  Serial.println(WiFi.dnsIP(0));
  Serial.print(F("DNS2: "));
  Serial.println(WiFi.dnsIP(1));

  // Informações do ESP
  Serial.println(F("--- HARDWARE ---"));
  Serial.print(F("Chip ID: "));
  Serial.println(ESP.getChipId(), HEX);
  Serial.print(F("Free Heap: "));
  Serial.print(getFreeHeap());
  Serial.println(F(" bytes"));
  Serial.print(F("Flash Size: "));
  Serial.print(ESP.getFlashChipSize());
  Serial.println(F(" bytes"));
  Serial.print(F("CPU Freq: "));
  Serial.print(ESP.getCpuFreqMHz());
  Serial.println(F(" MHz"));

  // Informações do projeto
  Serial.println(F("--- PROJETO ---"));
  Serial.print(F("Car ID: "));
  Serial.println(CAR_ID);
  Serial.print(F("WS Host: "));
  Serial.println(WS_HOST_STR);
  Serial.print(F("WS Port: "));
  Serial.println(WS_PORT);
  Serial.print(F("Connect Once: "));
  Serial.println(WS_CONNECT_ONCE ? F("SIM") : F("NÃO"));

  // Status dos pinos e periféricos
  Serial.println(F("--- PERIFÉRICOS ---"));
  Serial.print(F("Relay Pin: "));
  Serial.println(RELAY_PIN);
  Serial.print(F("Display DIO: "));
  Serial.println(TM1637_DIO);
  Serial.print(F("Display CLK: "));
  Serial.println(TM1637_CLK);

  // Módulo 74HC595
  Serial.println(F("--- 74HC595 SHIFT REGISTER ---"));
  Serial.print(F("Data Pin (D2): "));
  Serial.println(HC595_DATA_PIN);
  Serial.print(F("Latch Pin (D3): "));
  Serial.println(HC595_LATCH_PIN);
  Serial.print(F("Clock Pin (D4): "));
  Serial.println(HC595_CLOCK_PIN);
  Serial.print(F("Estado atual: 0b"));
  Serial.println(HC595::getByte(), BIN);

  Serial.println(F("======================================\n"));
}

namespace App
{
  // ===== DECLARAÇÕES DE FUNÇÕES =====
  void startOperation(int minutes);
  void stopOperation();
  void pauseOperation();
  void resumeOperation();
  void liberateWithoutTime();
  void updateDisplay();
  void updateTimeCounter();
  void handleOperationMessage(const String &message);
  void handleSessionData(const String &message);
  const char *getStatusString(OperationStatus status);

  void publishStatus(const char *status)
  {
    g_lastStatus = status;
    String json;
    json.reserve(128);
    json += "{\"type\":\"status\",";
    json += "\"carId\":\"";
    json += CAR_ID;
    json += "\",";
    json += "\"status\":\"";
    json += status;
    json += "\",";
    json += "\"relayOn\":";
    json += (Relay::isOn() ? "true" : "false");
    json += "}";
    webSocket.sendTXT(json);
    sessionSentFrames++;
    Disp::showStatus(status);
    if (LOG_VERBOSE)
    {
      Serial.print(F("[STATUS] carId="));
      Serial.print(CAR_ID);
      Serial.print(F(" status="));
      Serial.print(status);
      Serial.print(F(" relay="));
      Serial.println(Relay::isOn() ? F("ON") : F("OFF"));
    }
    if (LOG_STATUS_JSON)
      Serial.println(json);
  }

  void sendHello()
  {
    // Verifica se há heap suficiente antes de enviar mensagens
    if (getFreeHeap() < 8000)
    {
      Serial.print(F("[HELLO][ERRO] Heap baixo: "));
      Serial.print(getFreeHeap());
      Serial.println(F(" bytes - pulando hello"));
      return;
    }

    // Modo simples (texto puro) para gateways que ainda não parseiam JSON
#if defined(WS_HELLO_SIMPLE)
    {
      String plain = String("HELLO ") + CAR_ID;
      webSocket.sendTXT(plain);
      sessionSentFrames++;
      if (LOG_VERBOSE)
      {
        Serial.print(F("[HELLO][SIMPLE] enviado plain='"));
        Serial.print(plain);
        Serial.println('"');
      }
    }
    // Não envia JSON adicional neste modo.
#else
    // JSON padrão
    {
      String json;
      json.reserve(256);
      json += "{\"type\":\"hello\",";
      json += "\"carId\":\"";
      json += CAR_ID;
      json += "\",";
      json += "\"ip\":\"";
      json += Net::ip();
      json += "\",";
      json += "\"hostname\":\"";
      json += (Net::hostname() ? Net::hostname() : "");
      json += "\",";
      json += "\"board\":\"ESP8266\"";
      json += "}";
      webSocket.sendTXT(json);
      sessionSentFrames++;
      if (LOG_VERBOSE)
      {
        Serial.print(F("[HELLO][JSON] enviado size="));
        Serial.println(json.length());
      }
    }
#if defined(WS_HELLO_COMPAT) && !defined(WS_HELLO_SIMPLE)
    // Envia também linha de compatibilidade texto
    {
      String compat = String("HELLO-CAR ") + CAR_ID;
      webSocket.sendTXT(compat);
      sessionSentFrames++;
      if (LOG_VERBOSE)
      {
        Serial.print(F("[HELLO][COMPAT] enviado plain='"));
        Serial.print(compat);
        Serial.println('"');
      }
    }
#endif
#endif
    // Log único para identificar IP da placa no Serial Monitor (independente do modo)
    Serial.print(F("Device IP: "));
    Serial.println(Net::ip());
  }

  void printConnectionSnapshot()
  {
    String snap;
    snap.reserve(96);
    snap += "{\"carId\":\"";
    snap += CAR_ID;
    snap += "\",";
    snap += "\"online\":";
    snap += (webSocket.isConnected() ? "true" : "false");
    snap += ",";
    snap += "\"lastSeenSec\":";
    if (webSocket.isConnected() && lastInboundAt > 0)
    {
      snap += ((millis() - lastInboundAt) / 1000);
    }
    else
    {
      snap += "null";
    }
    snap += "}";
    Serial.println(snap);
  }

  void printDetailedSnapshot()
  {
    Serial.print(F("[SNAP] carId="));
    Serial.print(CAR_ID);
    Serial.print(F(" ip="));
    Serial.print(Net::ip());
    Serial.print(F(" rssi="));
    Serial.print(Net::rssi());
    Serial.print(F(" relay="));
    Serial.print(Relay::isOn() ? F("ON") : F("OFF"));
    Serial.print(F(" status="));
    Serial.print(g_lastStatus);
    Serial.print(F(" uptime_s="));
    Serial.print(millis() / 1000);
    Serial.print(F(" ws="));
    Serial.print(webSocket.isConnected() ? F("up") : F("down"));
    Serial.print(F(" lastSeen_s="));
    if (lastInboundAt)
      Serial.print((millis() - lastInboundAt) / 1000);
    else
      Serial.print(F("-"));
    Serial.print(F(" heap="));
    Serial.print(getFreeHeap());
    Serial.println();
  }

  void sendHeartbeat()
  {
    unsigned long now = millis();
    if (now - lastHeartbeatAt < HEARTBEAT_MS)
      return;
    lastHeartbeatAt = now;
    String json;
    json.reserve(256);
    json += "{\"type\":\"heartbeat\",";
    json += "\"carId\":\"";
    json += CAR_ID;
    json += "\",";
    json += "\"status\":\"";
    json += g_lastStatus;
    json += "\",";
    json += "\"relayOn\":";
    json += (Relay::isOn() ? "true" : "false");
    json += ",";
    json += "\"rssi\":";
    json += Net::rssi();
    json += ",";
    json += "\"ip\":\"";
    json += Net::ip();
    json += "\",";
    json += "\"uptimeSec\":";
    json += (now / 1000);
    json += ",";
    json += "\"heap\":";
    json += getFreeHeap();
    json += "}";
    webSocket.sendTXT(json);
    sessionSentFrames++;
    // Também imprime o snapshot para o Serial, como solicitado
    if (LOG_VERBOSE)
    {
      Serial.print(F("[HEARTBEAT] carId="));
      Serial.print(CAR_ID);
      Serial.print(F(" status="));
      Serial.print(g_lastStatus);
      Serial.print(F(" rssi="));
      Serial.print(Net::rssi());
      Serial.print(F(" relay="));
      Serial.print(Relay::isOn() ? F("ON") : F("OFF"));
      Serial.print(F(" uptime_s="));
      Serial.print(now / 1000);
      Serial.print(F(" heap="));
      Serial.print(getFreeHeap());
      Serial.println();
    }
    if (LOG_HEARTBEAT_JSON)
      Serial.println(json);
    // Snapshot simples (JSON compacto) pode ser mantido para integrações
    if (!LOG_VERBOSE)
      printConnectionSnapshot();
  }

  void handleAction(const String &action)
  {
    // Comandos simples (compatibilidade)
    if (action == "start")
    {
      g_operationStatus = OP_ACTIVE;
      startOperation(2); // 2 minutos padrão
      publishStatus("ACTIVE");
    }
    else if (action == "stop")
    {
      stopOperation();
      publishStatus("STOPPED");
    }
    else if (action == "pause")
    {
      pauseOperation();
      publishStatus("PAUSED");
    }
    else if (action == "resume")
    {
      resumeOperation();
      publishStatus("ACTIVE");
    }
    else if (action == "liberate_free")
    {
      liberateWithoutTime();
      publishStatus("LIBERATED_FREE");
    }
    else if (action == "emergency")
    {
      stopOperation();
      publishStatus("EMERGENCY");
    }
    // Comandos para controle do HC595 (mantidos)
    else if (action.startsWith("hc595_pin_"))
    {
      // Formato: hc595_pin_3_on ou hc595_pin_5_off
      int pinIndex = action.substring(10, 11).toInt(); // Extrai o número do pino
      bool state = action.endsWith("_on");

      if (pinIndex >= 0 && pinIndex <= 7)
      {
        HC595::setPin(pinIndex, state);
        HC595::update();
        Serial.print(F("[HC595] Pin "));
        Serial.print(pinIndex);
        Serial.println(state ? F(" ligado") : F(" desligado"));
      }
    }
    else if (action == "hc595_all_on")
    {
      HC595::allOn();
      Serial.println(F("[HC595] Todas as saídas ligadas"));
    }
    else if (action == "hc595_all_off")
    {
      HC595::allOff();
      Serial.println(F("[HC595] Todas as saídas desligadas"));
    }
    else if (action == "hc595_running_light")
    {
      HC595::runningLight(200);
      Serial.println(F("[HC595] Efeito running light executado"));
    }
    else if (action.startsWith("hc595_byte_"))
    {
      // Formato: hc595_byte_170 (para 0b10101010)
      uint8_t value = action.substring(11).toInt();
      HC595::setByte(value);
      HC595::update();
      Serial.print(F("[HC595] Byte definido: 0b"));
      Serial.println(value, BIN);
    }
  }

  // ===== FUNÇÕES DE CONTROLE DE OPERAÇÃO (movidas para dentro do namespace) =====

  // Converte OperationStatus para string para debug/display
  const char *getStatusString(OperationStatus status)
  {
    switch (status)
    {
    case OP_STOPPED:
      return "STOPPED";
    case OP_ACTIVE:
      return "ACTIVE";
    case OP_PAUSED:
      return "PAUSED";
    case OP_LIBERATED_TIME:
      return "LIBERATED_TIME";
    case OP_LIBERATED_FREE:
      return "LIBERATED_FREE";
    default:
      return "UNKNOWN";
    }
  }

  // Atualiza o contador de tempo (chamado no loop)
  void updateTimeCounter()
  {
    unsigned long now = millis();

    // Atualiza a cada 1 segundo
    if (now - g_lastCountUpdate >= 1000)
    {
      g_lastCountUpdate = now;

      // Só conta se estiver em modo ativo ou liberado com tempo
      if (g_operationStatus == OP_ACTIVE || g_operationStatus == OP_LIBERATED_TIME)
      {

        if (g_isCountingDown)
        {
          // Contagem regressiva
          if (g_remainingSeconds > 0)
          {
            g_remainingSeconds--;
          }
          else
          {
            // Chegou em 0:00, muda para contagem progressiva
            g_isCountingDown = false;
            g_extraSeconds = 0;
            Serial.println(F("[TIMER] Tempo esgotado! Iniciando contagem progressiva..."));
          }
        }
        else
        {
          // Contagem progressiva (tempo extra)
          g_extraSeconds++;
        }

        // Atualiza display
        updateDisplay();
      }
    }
  }

  // Atualiza o display com o tempo atual
  void updateDisplay()
  {
    if (g_operationStatus == OP_STOPPED)
    {
      Disp::showText("--:--");
      return;
    }

    if (g_operationStatus == OP_LIBERATED_FREE)
    {
      Disp::showText("FREE");
      return;
    }

    if (g_operationStatus == OP_PAUSED)
    {
      // Para pausado, pisca o tempo atual
      static bool blinkState = false;
      static unsigned long lastBlink = 0;

      if (millis() - lastBlink >= 500)
      { // Pisca a cada 500ms
        lastBlink = millis();
        blinkState = !blinkState;

        if (blinkState)
        {
          int displayMinutes, displaySeconds;
          if (g_isCountingDown)
          {
            displayMinutes = g_remainingSeconds / 60;
            displaySeconds = g_remainingSeconds % 60;
          }
          else
          {
            displayMinutes = g_extraSeconds / 60;
            displaySeconds = g_extraSeconds % 60;
          }
          char timeStr[8];
          sprintf(timeStr, "%02d:%02d", displayMinutes, displaySeconds);
          Serial.print(F("[DISPLAY] Pausado mostrando: "));
          Serial.println(timeStr);
          Disp::showText(timeStr);
        }
        else
        {
          Disp::showText("    "); // Espaços em branco para piscar
        }
      }
      return;
    }

    // Para os outros estados (ACTIVE, LIBERATED_TIME), mostra o tempo
    int displayMinutes, displaySeconds;

    if (g_isCountingDown)
    {
      // Contagem regressiva: mostra tempo restante
      displayMinutes = g_remainingSeconds / 60;
      displaySeconds = g_remainingSeconds % 60;
    }
    else
    {
      // Contagem progressiva: mostra tempo extra
      displayMinutes = g_extraSeconds / 60;
      displaySeconds = g_extraSeconds % 60;
    }

    char timeStr[8];
    sprintf(timeStr, "%02d:%02d", displayMinutes, displaySeconds);
    Serial.print(F("[DISPLAY] Atualizando: "));
    Serial.print(timeStr);
    Serial.print(F(" (Regressiva: "));
    Serial.print(g_isCountingDown ? "SIM" : "NAO");
    Serial.print(F(", Status: "));
    Serial.print(getStatusString(g_operationStatus));
    Serial.println(F(")"));
    Disp::showText(timeStr);
  }

  // Inicia uma operação com tempo
  void startOperation(int minutes)
  {
    g_initialMinutes = minutes;
    g_remainingSeconds = minutes * 60;
    g_extraSeconds = 0;
    g_isCountingDown = true;
    g_lastCountUpdate = millis();

    // Liga relay
    if (!g_relayState)
    {
      Relay::start();
      g_relayState = true;
    }

    Serial.print(F("[OPERATION] Iniciada com "));
    Serial.print(minutes);
    Serial.println(F(" minutos"));

    updateDisplay();
  }

  // Para a operação
  void stopOperation()
  {
    g_operationStatus = OP_STOPPED;
    g_remainingSeconds = 0;
    g_extraSeconds = 0;
    g_lastCountUpdate = 0;

    // Desliga relay
    if (g_relayState)
    {
      Relay::stop();
      g_relayState = false;
    }

    Serial.println(F("[OPERATION] Parada"));
    updateDisplay();
  }

  // Pausa/resume a operação
  void pauseOperation()
  {
    if (g_operationStatus == OP_ACTIVE || g_operationStatus == OP_LIBERATED_TIME)
    {
      g_operationStatus = OP_PAUSED;
      Serial.println(F("[OPERATION] Pausada"));
    }
  }

  void resumeOperation()
  {
    if (g_operationStatus == OP_PAUSED)
    {
      // Volta para o estado anterior (precisa melhorar esta lógica)
      g_operationStatus = OP_ACTIVE; // Assumindo que era ACTIVE
      g_lastCountUpdate = millis();  // Reset timer
      Serial.println(F("[OPERATION] Resumida"));
    }
  }

  // Libera sem tempo (FREE)
  void liberateWithoutTime()
  {
    g_operationStatus = OP_LIBERATED_FREE;
    g_remainingSeconds = 0;
    g_extraSeconds = 0;

    // Liga relay
    if (!g_relayState)
    {
      Relay::start();
      g_relayState = true;
    }

    Serial.println(F("[OPERATION] Liberada sem tempo"));
    updateDisplay();
  }

  // Processa mensagens JSON com dados da operação
  void handleOperationMessage(const String &message)
  {
    // Parse simples do JSON para extrair campos necessários
    String carId = "";
    String status = "";
    int initialMinutes = 0;
    int remainingSeconds = 0;

    // Extrair carId
    int carIndex = message.indexOf("\"carId\"");
    if (carIndex >= 0)
    {
      int colon = message.indexOf(':', carIndex);
      int q1 = message.indexOf('"', colon + 1);
      int q2 = message.indexOf('"', q1 + 1);
      if (q1 >= 0 && q2 > q1)
      {
        carId = message.substring(q1 + 1, q2);
      }
    }

    // Extrair status
    int statusIndex = message.indexOf("\"status\"");
    if (statusIndex >= 0)
    {
      int colon = message.indexOf(':', statusIndex);
      int q1 = message.indexOf('"', colon + 1);
      int q2 = message.indexOf('"', q1 + 1);
      if (q1 >= 0 && q2 > q1)
      {
        status = message.substring(q1 + 1, q2);
      }
    }

    // Extrair initialMinutes
    int minutesIndex = message.indexOf("\"initialMinutes\"");
    if (minutesIndex >= 0)
    {
      int colon = message.indexOf(':', minutesIndex);
      int comma = message.indexOf(',', colon);
      int brace = message.indexOf('}', colon);
      int end = (comma > 0 && comma < brace) ? comma : brace;
      if (end > colon)
      {
        String minutesStr = message.substring(colon + 1, end);
        minutesStr.trim();
        initialMinutes = minutesStr.toInt();
      }
    }

    // Extrair remainingTime.total_seconds
    int remainingIndex = message.indexOf("\"total_seconds\"");
    if (remainingIndex >= 0)
    {
      int colon = message.indexOf(':', remainingIndex);
      int comma = message.indexOf(',', colon);
      int brace = message.indexOf('}', colon);
      int end = (comma > 0 && comma < brace) ? comma : brace;
      if (end > colon)
      {
        String secondsStr = message.substring(colon + 1, end);
        secondsStr.trim();
        remainingSeconds = secondsStr.toInt();
      }
    }

    // Validar carId (só processa se for o carro correto)
    if (carId.length() > 0 && carId != CAR_ID)
    {
      Serial.print(F("[OPERATION] Mensagem ignorada - carId: "));
      Serial.print(carId);
      Serial.print(F(" (esperado: "));
      Serial.print(CAR_ID);
      Serial.println(F(")"));
      return;
    }

    // Processar comandos baseados no status
    Serial.print(F("[OPERATION] Recebido - Status: "));
    Serial.print(status);
    Serial.print(F(" Minutos: "));
    Serial.print(initialMinutes);
    Serial.print(F(" Restantes: "));
    Serial.print(remainingSeconds);
    Serial.println(F("s"));

    if (status == "ACTIVE")
    {
      g_operationStatus = OP_ACTIVE;
      if (initialMinutes > 0)
      {
        startOperation(initialMinutes);
      }
      else if (remainingSeconds > 0)
      {
        // Sincronizar com tempo restante
        g_remainingSeconds = remainingSeconds;
        g_isCountingDown = true;
        if (!g_relayState)
        {
          Relay::start();
          g_relayState = true;
        }
        updateDisplay();
      }
      publishStatus("ACTIVE");
    }
    else if (status == "PAUSED")
    {
      pauseOperation();
      publishStatus("PAUSED");
    }
    else if (status == "STOPPED")
    {
      stopOperation();
      publishStatus("STOPPED");
    }
    else if (status == "LIBERATED")
    {
      // Verifica se tem tempo ou não
      if (initialMinutes > 0 || remainingSeconds > 0)
      {
        g_operationStatus = OP_LIBERATED_TIME;
        if (initialMinutes > 0)
        {
          startOperation(initialMinutes);
        }
        else
        {
          g_remainingSeconds = remainingSeconds;
          g_isCountingDown = true;
          if (!g_relayState)
          {
            Relay::start();
            g_relayState = true;
          }
          updateDisplay();
        }
        publishStatus("LIBERATED_TIME");
      }
      else
      {
        liberateWithoutTime();
        publishStatus("LIBERATED_FREE");
      }
    }
  }

  // Processa mensagens session_data com estrutura aninhada
  void handleSessionData(const String &message)
  {
    Serial.println(F("[SESSION_DATA] Processando mensagem"));

    // Extrair dados da estrutura aninhada "data"
    int dataStart = message.indexOf("\"data\":");
    if (dataStart < 0)
    {
      Serial.println(F("[SESSION_DATA] Campo 'data' não encontrado"));
      return;
    }

    // Encontrar o início do objeto data
    int dataObjStart = message.indexOf('{', dataStart);
    if (dataObjStart < 0)
      return;

    // Extrair a substring do objeto data
    int braceCount = 1;
    int dataObjEnd = dataObjStart + 1;
    while (dataObjEnd < message.length() && braceCount > 0)
    {
      if (message.charAt(dataObjEnd) == '{')
        braceCount++;
      else if (message.charAt(dataObjEnd) == '}')
        braceCount--;
      dataObjEnd++;
    }

    String dataObj = message.substring(dataObjStart, dataObjEnd);
    Serial.print(F("[SESSION_DATA] Objeto data extraído: "));
    Serial.println(dataObj);

    // Parse do objeto data
    String carId = "";
    String status = "";
    int initialMinutes = 0;
    int remainingSeconds = 0;

    // Extrair carId do objeto data
    int carIndex = dataObj.indexOf("\"carId\"");
    if (carIndex >= 0)
    {
      int colon = dataObj.indexOf(':', carIndex);
      int q1 = dataObj.indexOf('"', colon + 1);
      int q2 = dataObj.indexOf('"', q1 + 1);
      if (q1 >= 0 && q2 > q1)
      {
        carId = dataObj.substring(q1 + 1, q2);
      }
    }

    // Extrair status do objeto data
    int statusIndex = dataObj.indexOf("\"status\"");
    if (statusIndex >= 0)
    {
      int colon = dataObj.indexOf(':', statusIndex);
      int q1 = dataObj.indexOf('"', colon + 1);
      int q2 = dataObj.indexOf('"', q1 + 1);
      if (q1 >= 0 && q2 > q1)
      {
        status = dataObj.substring(q1 + 1, q2);
      }
    }

    // Extrair initialMinutes do objeto data
    int minutesIndex = dataObj.indexOf("\"initialMinutes\"");
    if (minutesIndex >= 0)
    {
      int colon = dataObj.indexOf(':', minutesIndex);
      int comma = dataObj.indexOf(',', colon);
      int brace = dataObj.indexOf('}', colon);
      int end = (comma > 0 && comma < brace) ? comma : brace;
      if (end > colon)
      {
        String minutesStr = dataObj.substring(colon + 1, end);
        minutesStr.trim();
        initialMinutes = minutesStr.toInt();
      }
    }

    // Extrair total_seconds do objeto data
    int remainingIndex = dataObj.indexOf("\"total_seconds\"");
    if (remainingIndex >= 0)
    {
      int colon = dataObj.indexOf(':', remainingIndex);
      int comma = dataObj.indexOf(',', colon);
      int brace = dataObj.indexOf('}', colon);
      int end = (comma > 0 && comma < brace) ? comma : brace;
      if (end > colon)
      {
        String secondsStr = dataObj.substring(colon + 1, end);
        secondsStr.trim();
        remainingSeconds = secondsStr.toInt();
      }
    }

    // Validar carId
    if (carId.length() > 0 && carId != CAR_ID)
    {
      Serial.print(F("[SESSION_DATA] Mensagem ignorada - carId: "));
      Serial.print(carId);
      Serial.print(F(" (esperado: "));
      Serial.print(CAR_ID);
      Serial.println(F(")"));
      return;
    }

    // Log dos dados extraídos
    Serial.print(F("[SESSION_DATA] carId: "));
    Serial.print(carId);
    Serial.print(F(" status: "));
    Serial.print(status);
    Serial.print(F(" initialMinutes: "));
    Serial.print(initialMinutes);
    Serial.print(F(" remainingSeconds: "));
    Serial.println(remainingSeconds);

    // Processar baseado no status
    if (status == "ACTIVE")
    {
      g_operationStatus = OP_ACTIVE;
      if (initialMinutes > 0)
      {
        startOperation(initialMinutes);
      }
      else if (remainingSeconds > 0)
      {
        // Sincronizar com tempo restante recebido
        g_remainingSeconds = remainingSeconds;
        g_isCountingDown = true;
        g_lastCountUpdate = millis();
        if (!g_relayState)
        {
          Relay::start();
          g_relayState = true;
        }
        updateDisplay();
        Serial.print(F("[SESSION_DATA] Sincronizado com "));
        Serial.print(remainingSeconds);
        Serial.println(F(" segundos restantes"));
      }
      publishStatus("ACTIVE");
    }
    else if (status == "PAUSED")
    {
      pauseOperation();
      publishStatus("PAUSED");
    }
    else if (status == "STOPPED")
    {
      stopOperation();
      publishStatus("STOPPED");
    }
    else if (status == "LIBERATED")
    {
      if (initialMinutes > 0 || remainingSeconds > 0)
      {
        g_operationStatus = OP_LIBERATED_TIME;
        if (initialMinutes > 0)
        {
          startOperation(initialMinutes);
        }
        else
        {
          g_remainingSeconds = remainingSeconds;
          g_isCountingDown = true;
          g_lastCountUpdate = millis();
          if (!g_relayState)
          {
            Relay::start();
            g_relayState = true;
          }
          updateDisplay();
        }
        publishStatus("LIBERATED_TIME");
      }
      else
      {
        liberateWithoutTime();
        publishStatus("LIBERATED_FREE");
      }
    }
  }
}

void webSocketEvent(WStype_t type, uint8_t *payload, size_t length)
{
  switch (type)
  {
  case WStype_CONNECTED:
    lastInboundAt = millis();
    currentSessionStartedAt = millis();
    sessionSentFrames = 0;
    sessionRecvFrames = 0;
    wsInHandshake = false;
    wsNextAllowedConnectAt = millis() + WS_BASE_RETRY_MS; // pequena janela pós conexão

    // Sempre aguardar um pouco antes de enviar mensagens para estabilizar
    pendingHelloSend = true;
    pendingHelloScheduledAt = millis();

    Serial.print(F("[WS][CONNECT] Heap livre: "));
    Serial.print(getFreeHeap());
    Serial.println(F(" bytes"));
    App::printConnectionSnapshot();
    Serial.println(F("[WS] STATE: CONNECTED"));
    break;
  case WStype_DISCONNECTED:
    Serial.print(F("[WS][DISCONNECT] Código: 1006 (conexão anormal) - Heap: "));
    Serial.print(getFreeHeap());
    Serial.println(F(" bytes"));

    if (LOG_VERBOSE)
      App::printDetailedSnapshot();
    else
      App::printConnectionSnapshot();
    Serial.print(F("[WS][INFO] host_atual="));
    Serial.println(ALT_WS_HOSTS[currentHostIndex] ? ALT_WS_HOSTS[currentHostIndex] : "(null)");
    if (currentSessionStartedAt)
    {
      unsigned long dur = millis() - currentSessionStartedAt;
      Serial.print(F("[WS][SESSION] dur_ms="));
      Serial.print(dur);
      Serial.print(F(" sent="));
      Serial.print(sessionSentFrames);
      Serial.print(F(" recv="));
      Serial.print(sessionRecvFrames);
      Serial.print(F(" lastSeenDelta_s="));
      if (lastInboundAt)
        Serial.print((millis() - lastInboundAt) / 1000);
      else
        Serial.print(F("-"));
      Serial.println();
    }
    if (WS_CONNECT_ONCE)
    {
      Serial.println(F("[WS] STATE: DISCONNECTED (modo conexão única – não reconectará)"));
    }
    else
    {
      Serial.println(F("[WS] STATE: DISCONNECTED (tentará reconectar)"));
      {
        static unsigned long currentDelay = WS_BASE_RETRY_MS;
        currentDelay = (currentDelay == 0) ? WS_BASE_RETRY_MS : (currentDelay * 2);
        if (currentDelay > WS_MAX_RETRY_MS)
          currentDelay = WS_MAX_RETRY_MS;
        wsNextAllowedConnectAt = millis() + currentDelay;
        if (LOG_VERBOSE)
        {
          Serial.print(F("[WS][BACKOFF] Próxima tentativa em "));
          Serial.print(currentDelay);
          Serial.println(F(" ms"));
        }
        // Rotaciona host para próxima alternativa, evitando self-ip
        WSUtils::rotateHost(ALT_WS_HOSTS, ALT_WS_HOSTS_COUNT, currentHostIndex, true);
        if (LOG_VERBOSE)
        {
          Serial.print(F("[WS][BACKOFF] Próximo host: "));
          Serial.println(ALT_WS_HOSTS[currentHostIndex] ? ALT_WS_HOSTS[currentHostIndex] : "(null)");
        }
      }
    }
    wsInHandshake = false;
    break;
  case WStype_TEXT:
  {
    lastInboundAt = millis();
    sessionRecvFrames++;
    String msg;
    msg.reserve(length);
    for (size_t i = 0; i < length; i++)
      msg += (char)payload[i];

    // Detectar tipo de mensagem
    if (msg.indexOf("\"action\"") >= 0)
    {
      // Mensagem de comando simples
      int idx = msg.indexOf("\"action\"");
      int colon = msg.indexOf(':', idx);
      int q1 = msg.indexOf('"', colon + 1);
      int q2 = msg.indexOf('"', q1 + 1);
      if (q1 >= 0 && q2 > q1)
      {
        String action = msg.substring(q1 + 1, q2);
        App::handleAction(action);
      }
    }
    else if (msg.indexOf("\"type\":\"session_data\"") >= 0)
    {
      // Mensagem session_data com estrutura aninhada
      Serial.println(F("[WS] Recebido session_data"));
      App::handleSessionData(msg);
    }
    else if (msg.indexOf("\"carId\"") >= 0 && msg.indexOf("\"status\"") >= 0)
    {
      // Mensagem de dados da operação direta (formato antigo)
      App::handleOperationMessage(msg);
    }
    else if (msg.indexOf("\"type\"") >= 0)
    {
      // Outras mensagens do sistema (hello, etc) - só loggar
      Serial.print(F("[WS] Mensagem do sistema: "));
      Serial.println(msg);
    }
    else
    {
      // Mensagem desconhecida
      Serial.print(F("[WS] Mensagem desconhecida: "));
      Serial.println(msg);
    }
    break;
  }
  case WStype_PING:
    lastInboundAt = millis();
    sessionRecvFrames++;
    if (LOG_VERBOSE)
      Serial.println(F("[WS] PING recebido"));
    break;
  case WStype_PONG:
    lastInboundAt = millis();
    sessionRecvFrames++;
    if (LOG_VERBOSE)
      Serial.println(F("[WS] PONG recebido"));
    break;
  case WStype_ERROR:
    if (LOG_VERBOSE)
    {
      Serial.print(F("[WS][ERRO] Evento erro length="));
      Serial.println(length);
      Serial.print(F("[WS][ERRO] host_atual="));
      Serial.println(ALT_WS_HOSTS[currentHostIndex] ? ALT_WS_HOSTS[currentHostIndex] : "(null)");
    }
    break;
  default:
    break;
  }
}

// Healthcheck HTTP para diagnosticar se o gateway está acessível pela rede
void tryHttpHealthcheck()
{
  // Se em modo WS_CONNECT_ONCE e já desistimos, não fazer healthcheck
  if (WS_CONNECT_ONCE && wsConnectGaveUp)
    return;

  // Usa sempre o host atual (pode estar em fallback)
  const char *host = ALT_WS_HOSTS[currentHostIndex];
  if (!host || strlen(host) == 0)
    return; // sem host configurado
  if (WSUtils::isSelfHost(host))
  {
    // Evita tentativa de healthcheck contra a própria placa
    if (LOG_VERBOSE)
      Serial.println(F("[HTTP][HEALTH][SKIP] host==self-ip; ajuste WS_HOST_STR para IP do gateway"));
    return;
  }
  unsigned long now = millis();
  if (now - lastHealthcheckAt < 15000) // a cada 15s no máximo
    return;
  if (webSocket.isConnected())
    return; // só faz healthcheck se WS não conectado
  lastHealthcheckAt = now;
  WiFiClient client;
  HTTPClient http;
  String url = String("http://") + host + ":" + WS_PORT + "/api/ws/health";
  if (http.begin(client, url))
  {
    int code = http.GET();
    if (code > 0)
    {
      String payload = http.getString();
      Serial.print(F("[HTTP][HEALTH] status="));
      Serial.print(code);
      Serial.print(F(" body="));
      Serial.println(payload);
    }
    else
    {
      Serial.print(F("[HTTP][HEALTH][ERRO] code="));
      Serial.println(code);
    }
    http.end();
  }
  else
  {
    Serial.println(F("[HTTP][HEALTH][ERRO] Falha em iniciar conexão (http.begin)"));
  }
}

void startWebSocket()
{
  unsigned long now = millis();
  if (now < wsNextAllowedConnectAt)
  {
    if (LOG_VERBOSE)
    {
      Serial.print(F("[WS][BACKOFF] Aguardando "));
      Serial.print(wsNextAllowedConnectAt - now);
      Serial.println(F(" ms para nova tentativa"));
    }
    return;
  }
  const char *host = ALT_WS_HOSTS[currentHostIndex];
  if (!host || strlen(host) == 0)
  {
    Serial.println(F("[WS][ERRO] Host vazio ao iniciar."));
    return;
  }
  if (WSUtils::isSelfHost(host))
  {
    Serial.print(F("[WS][SKIP] Host é o IP da própria placa ("));
    Serial.print(host);
    Serial.println(F(") — rotacionando"));
    WSUtils::rotateHost(ALT_WS_HOSTS, ALT_WS_HOSTS_COUNT, currentHostIndex, true);
    host = ALT_WS_HOSTS[currentHostIndex];
    if (!host || !*host || WSUtils::isSelfHost(host))
    {
      Serial.println(F("[WS][ERRO] Sem host válido diferente do IP da placa. Verifique WS_HOST_STR."));
      if (WS_CONNECT_ONCE)
      {
        wsConnectGaveUp = true;
        Serial.println(F("[WS][CONNECT_ONCE] Parando tentativas - configure WS_HOST_STR corretamente."));
      }
      return;
    }
  }
  Serial.print(F("[WS] Conectando a ws://"));
  Serial.print(host);
  Serial.print(":");
  Serial.print(WS_PORT);
  Serial.print(F("/ws?carId="));
  Serial.println(CAR_ID);
  String path = String("/ws?carId=") + CAR_ID;
  webSocket.begin(host, WS_PORT, path.c_str());
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);       // 5s entre reconexões
  webSocket.enableHeartbeat(30000, 10000, 2); // ping a cada 30s, timeout 10s, 2 tentativas
  lastWsConnectAttemptAt = millis();
  wsConnectAttempts = 1;
  wsInHandshake = true;
  wsHandshakeStartedAt = lastWsConnectAttemptAt;
}
void setup()
{
  Serial.begin(115200);
#ifdef ESP8266
  Serial.setDebugOutput(false); // evita logs extras da stack WiFi/SDK no terminal
#endif
  Relay::begin();
  Disp::begin();
  HC595::begin();

  // Teste rápido do HC595 na inicialização
  Serial.println(F("[HC595] Executando teste inicial..."));
  HC595::runningLight(100); // Efeito de LED correndo
  delay(500);
  HC595::setByte(0b10101010); // Padrão alternado
  HC595::update();
  delay(1000);
  HC595::allOff(); // Desliga tudo
  Serial.println(F("[HC595] Teste concluído"));

  Net::begin(ssid, password);
  // Configuração opcional de IP estático (defina nas build_flags):
#ifdef STATIC_IP_ADDR
  Net::configureStaticIp(STATIC_IP_ADDR, STATIC_IP_GW, STATIC_IP_MASK,
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
  Serial.println(F("Conectando ao WiFi..."));
  if (!Net::waitConnected(15000))
  { // espera até 15s
    Serial.println(F("Falha ao conectar no WiFi dentro do timeout."));
  }
  else
  {
    // Mostra informações completas do sistema após conectar WiFi
    showSystemInfo();
  }
  Net::setupTime();
  // mDNS desabilitado por preferência: usaremos apenas IP

  if (strlen(WS_HOST_STR) == 0)
  {
    Serial.println(F("[WS][ERRO] WS_HOST_STR não definido. Ajuste build_flags em platformio.ini"));
  }
  else
  {
    startWebSocket();
  }
  Disp::showStatus("STOPPED");
}

void loop()
{
  Net::loop();
  webSocket.loop();
  // Envio atrasado do HELLO/STATUS se configurado
  if (pendingHelloSend && (millis() - pendingHelloScheduledAt >= (unsigned long)WS_HELLO_DELAY_MS))
  {
    pendingHelloSend = false;
    App::sendHello();
    App::publishStatus("STOPPED");
  }
  // Heartbeat customizado temporariamente desabilitado para debug
  // #ifndef WS_DISABLE_HEARTBEAT
  //  App::sendHeartbeat();
  // #endif
  Disp::loop();

  // Atualiza contador de tempo da operação
  App::updateTimeCounter();

  // Se ainda não conectou após várias tentativas, imprime diagnóstico periódico
  if (!webSocket.isConnected() && strlen(WS_HOST_STR) != 0)
  {
    unsigned long now = millis();
    // Timeout de handshake: aborta se excedeu
    if (wsInHandshake && (now - wsHandshakeStartedAt > WS_HANDSHAKE_TIMEOUT_MS))
    {
      if (LOG_VERBOSE)
        Serial.println(F("[WS][TIMEOUT] Handshake excedeu limite, abortando e aplicando backoff base"));
      wsInHandshake = false;
      if (WS_CONNECT_ONCE)
      {
        // Em modo conexão única, não reagendar nem rotacionar
        wsConnectGaveUp = true;
        Serial.println(F("[WS][TIMEOUT] Modo conexão única – não haverá nova tentativa"));
      }
      else
      {
        wsNextAllowedConnectAt = now + WS_BASE_RETRY_MS;
        // Rotaciona host após timeout do handshake
        WSUtils::rotateHost(ALT_WS_HOSTS, ALT_WS_HOSTS_COUNT, currentHostIndex, true);
        if (LOG_VERBOSE)
        {
          Serial.print(F("[WS][TIMEOUT] Próximo host: "));
          Serial.println(ALT_WS_HOSTS[currentHostIndex] ? ALT_WS_HOSTS[currentHostIndex] : "(null)");
        }
      }
    }
    // Tentar iniciar (ou reiniciar) conexão quando permitido e não em handshake
    if (!wsInHandshake && now >= wsNextAllowedConnectAt)
    {
      if (WS_CONNECT_ONCE && (wsConnectAttempts >= 1 || wsConnectGaveUp))
      {
        // Já tentamos uma vez ou desistimos; não tentar novamente
        return;
      }
      if (LOG_VERBOSE)
      {
        Serial.print(F("[WS][DEBUG] Tentando conectar. RSSI="));
        Serial.print(Net::rssi());
        Serial.print(F(" IP="));
        Serial.println(Net::ip());
      }
      startWebSocket();
    }
    tryHttpHealthcheck();
  }
  // Comandos simples pelo Serial
  if (Serial.available())
  {
    char c = Serial.read();
    switch (c)
    {
    case 'i':
      App::printDetailedSnapshot();
      break;
    case 'j':
      App::printConnectionSnapshot();
      break;
    case 'h':
      Serial.println(F("Comandos: i= snapshot detalhado, j= snapshot json, h= ajuda"));
      break;
    }
  }
  // Pequeno descanso para não saturar a simulação/terminal do Wokwi
  delay(LOOP_DELAY_MS); // 10-20ms é seguro; use 5-10ms para testes de performance do WS
}
