#include "websocket_manager.h"
#include "../Config/config.h"
#include "../Operation/operation_manager.h"
#include "../Wifi/wifi.h"
#include "../Display/Display.h"
#include "../Reley/reley.h"
#include "../WS/WSUtils.h"
#include <ESP8266HTTPClient.h>

static WebSocketsClient g_webSocket;
static unsigned long g_lastHeartbeatAt = 0;
static size_t g_currentHostIndex = 0;

// Função utilitária para heap livre
static inline uint32_t getFreeHeap()
{
#if defined(ESP8266) || defined(ESP32)
    return ESP.getFreeHeap();
#else
    return 0;
#endif
}

namespace WebSocketManager
{

    void initialize()
    {
        // Configurações já são feitas na startConnection()
    }

    WebSocketsClient &getClient()
    {
        return g_webSocket;
    }

    bool isConnected()
    {
        return g_webSocket.isConnected();
    }

    void sendHello()
    {
        if (getFreeHeap() < 8000)
        {
            Serial.print(F("[HELLO][ERRO] Heap baixo: "));
            Serial.print(getFreeHeap());
            Serial.println(F(" bytes - pulando hello"));
            return;
        }

        // Modo simples para compatibilidade
#if defined(WS_HELLO_SIMPLE)
        String plain = String("HELLO ") + CAR_ID_STR;
        g_webSocket.sendTXT(plain);
        Operation::getState().sessionSentFrames++;

        if (LOG_VERBOSE)
        {
            Serial.print(F("[HELLO][SIMPLE] enviado plain='"));
            Serial.print(plain);
            Serial.println('"');
        }
#else
        // JSON padrão
        String json;
        json.reserve(256);
        json += "{\"type\":\"hello\",";
        json += "\"carId\":\"";
        json += CAR_ID_STR;
        json += "\",";
        json += "\"ip\":\"";
        json += Net::ip();
        json += "\",";
        json += "\"hostname\":\"";
        json += (Net::hostname() ? Net::hostname() : "");
        json += "\",";
        json += "\"board\":\"ESP8266\"";
        json += "}";

        g_webSocket.sendTXT(json);
        Operation::getState().sessionSentFrames++;

        if (LOG_VERBOSE)
        {
            Serial.print(F("[HELLO][JSON] enviado size="));
            Serial.println(json.length());
        }
#endif

        Serial.print(F("Device IP: "));
        Serial.println(Net::ip());
    }

    void publishStatus(const char *status)
    {
        Operation::getState().lastStatus = status;

        String json;
        json.reserve(128);
        json += "{\"type\":\"status\",";
        json += "\"carId\":\"";
        json += CAR_ID_STR;
        json += "\",";
        json += "\"status\":\"";
        json += status;
        json += "\",";
        json += "\"relayOn\":";
        json += (Relay::isOn() ? "true" : "false");
        json += "}";

        g_webSocket.sendTXT(json);
        Operation::getState().sessionSentFrames++;

        Disp::showStatus(status);

        if (LOG_VERBOSE)
        {
            Serial.print(F("[STATUS] carId="));
            Serial.print(CAR_ID_STR);
            Serial.print(F(" status="));
            Serial.print(status);
            Serial.print(F(" relay="));
            Serial.println(Relay::isOn() ? F("ON") : F("OFF"));
        }

        if (LOG_STATUS_JSON)
        {
            Serial.println(json);
        }
    }

    void sendHeartbeat()
    {
        unsigned long now = millis();
        if (now - g_lastHeartbeatAt < HEARTBEAT_MS)
        {
            return;
        }

        g_lastHeartbeatAt = now;

        String json;
        json.reserve(256);
        json += "{\"type\":\"heartbeat\",";
        json += "\"carId\":\"";
        json += CAR_ID_STR;
        json += "\",";
        json += "\"status\":\"";
        json += Operation::getState().lastStatus;
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

        g_webSocket.sendTXT(json);
        Operation::getState().sessionSentFrames++;

        if (LOG_VERBOSE)
        {
            Serial.print(F("[HEARTBEAT] carId="));
            Serial.print(CAR_ID_STR);
            Serial.print(F(" status="));
            Serial.print(Operation::getState().lastStatus);
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
        {
            Serial.println(json);
        }

        if (!LOG_VERBOSE)
        {
            printConnectionSnapshot();
        }
    }

    void printConnectionSnapshot()
    {
        String snap;
        snap.reserve(96);
        snap += "{\"carId\":\"";
        snap += CAR_ID_STR;
        snap += "\",";
        snap += "\"online\":";
        snap += (g_webSocket.isConnected() ? "true" : "false");
        snap += ",";
        snap += "\"lastSeenSec\":";

        if (g_webSocket.isConnected() && Operation::getState().lastInboundAt > 0)
        {
            snap += ((millis() - Operation::getState().lastInboundAt) / 1000);
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
        Serial.print(CAR_ID_STR);
        Serial.print(F(" ip="));
        Serial.print(Net::ip());
        Serial.print(F(" rssi="));
        Serial.print(Net::rssi());
        Serial.print(F(" relay="));
        Serial.print(Relay::isOn() ? F("ON") : F("OFF"));
        Serial.print(F(" status="));
        Serial.print(Operation::getState().lastStatus);
        Serial.print(F(" uptime_s="));
        Serial.print(millis() / 1000);
        Serial.print(F(" ws="));
        Serial.print(g_webSocket.isConnected() ? F("up") : F("down"));
        Serial.print(F(" lastSeen_s="));

        if (Operation::getState().lastInboundAt)
        {
            Serial.print((millis() - Operation::getState().lastInboundAt) / 1000);
        }
        else
        {
            Serial.print(F("-"));
        }

        Serial.print(F(" heap="));
        Serial.print(getFreeHeap());
        Serial.println();
    }

    void onEvent(WStype_t type, uint8_t *payload, size_t length)
    {
        auto &state = Operation::getState();

        switch (type)
        {
        case WStype_CONNECTED:
            state.lastInboundAt = millis();
            state.currentSessionStartedAt = millis();
            state.sessionSentFrames = 0;
            state.sessionRecvFrames = 0;
            state.wsInHandshake = false;
            state.wsNextAllowedConnectAt = millis() + WS_BASE_RETRY_MS;

            state.pendingHelloSend = true;
            state.pendingHelloScheduledAt = millis();

            Serial.print(F("[WS][CONNECT] Heap livre: "));
            Serial.print(getFreeHeap());
            Serial.println(F(" bytes"));
            printConnectionSnapshot();
            Serial.println(F("[WS] STATE: CONNECTED"));
            break;

        case WStype_DISCONNECTED:
            Serial.print(F("[WS][DISCONNECT] Código: 1006 (conexão anormal) - Heap: "));
            Serial.print(getFreeHeap());
            Serial.println(F(" bytes"));

            if (LOG_VERBOSE)
            {
                printDetailedSnapshot();
            }
            else
            {
                printConnectionSnapshot();
            }

            Serial.print(F("[WS][INFO] host_atual="));
            Serial.println(ALT_WS_HOSTS[g_currentHostIndex] ? ALT_WS_HOSTS[g_currentHostIndex] : "(null)");

            if (state.currentSessionStartedAt)
            {
                unsigned long dur = millis() - state.currentSessionStartedAt;
                Serial.print(F("[WS][SESSION] dur_ms="));
                Serial.print(dur);
                Serial.print(F(" sent="));
                Serial.print(state.sessionSentFrames);
                Serial.print(F(" recv="));
                Serial.print(state.sessionRecvFrames);
                Serial.println();
            }

            if (WS_CONNECT_ONCE)
            {
                Serial.println(F("[WS] STATE: DISCONNECTED (modo conexão única – não reconectará)"));
            }
            else
            {
                Serial.println(F("[WS] STATE: DISCONNECTED (tentará reconectar)"));

                static unsigned long currentDelay = WS_BASE_RETRY_MS;
                currentDelay = (currentDelay == 0) ? WS_BASE_RETRY_MS : (currentDelay * 2);
                if (currentDelay > WS_MAX_RETRY_MS)
                {
                    currentDelay = WS_MAX_RETRY_MS;
                }

                state.wsNextAllowedConnectAt = millis() + currentDelay;

                if (LOG_VERBOSE)
                {
                    Serial.print(F("[WS][BACKOFF] Próxima tentativa em "));
                    Serial.print(currentDelay);
                    Serial.println(F(" ms"));
                }

                WSUtils::rotateHost(ALT_WS_HOSTS, ALT_WS_HOSTS_COUNT, g_currentHostIndex, true);

                if (LOG_VERBOSE)
                {
                    Serial.print(F("[WS][BACKOFF] Próximo host: "));
                    Serial.println(ALT_WS_HOSTS[g_currentHostIndex] ? ALT_WS_HOSTS[g_currentHostIndex] : "(null)");
                }
            }

            state.wsInHandshake = false;
            break;

        case WStype_TEXT:
        {
            state.lastInboundAt = millis();
            state.sessionRecvFrames++;

            String msg;
            msg.reserve(length);
            for (size_t i = 0; i < length; i++)
            {
                msg += (char)payload[i];
            }

            // Processar mensagem
            if (msg.indexOf("\"action\"") >= 0)
            {
                int idx = msg.indexOf("\"action\"");
                int colon = msg.indexOf(':', idx);
                int q1 = msg.indexOf('"', colon + 1);
                int q2 = msg.indexOf('"', q1 + 1);

                if (q1 >= 0 && q2 > q1)
                {
                    String action = msg.substring(q1 + 1, q2);
                    Operation::handleAction(action);
                }
            }
            else if (msg.indexOf("\"type\":\"session_data\"") >= 0)
            {
                Serial.println(F("[WS] Recebido session_data"));
                Operation::handleSessionData(msg);
            }
            else if (msg.indexOf("\"carId\"") >= 0 && msg.indexOf("\"status\"") >= 0)
            {
                Operation::handleOperationMessage(msg);
            }
            else if (msg.indexOf("\"type\"") >= 0)
            {
                Serial.print(F("[WS] Mensagem do sistema: "));
                Serial.println(msg);
            }
            else
            {
                Serial.print(F("[WS] Mensagem desconhecida: "));
                Serial.println(msg);
            }
            break;
        }

        case WStype_PING:
            state.lastInboundAt = millis();
            state.sessionRecvFrames++;
            if (LOG_VERBOSE)
            {
                Serial.println(F("[WS] PING recebido"));
            }
            break;

        case WStype_PONG:
            state.lastInboundAt = millis();
            state.sessionRecvFrames++;
            if (LOG_VERBOSE)
            {
                Serial.println(F("[WS] PONG recebido"));
            }
            break;

        case WStype_ERROR:
            if (LOG_VERBOSE)
            {
                Serial.print(F("[WS][ERRO] Evento erro length="));
                Serial.println(length);
                Serial.print(F("[WS][ERRO] host_atual="));
                Serial.println(ALT_WS_HOSTS[g_currentHostIndex] ? ALT_WS_HOSTS[g_currentHostIndex] : "(null)");
            }
            break;

        default:
            break;
        }
    }

    void tryHttpHealthcheck()
    {
        auto &state = Operation::getState();

        if (WS_CONNECT_ONCE && state.wsConnectGaveUp)
        {
            return;
        }

        const char *host = ALT_WS_HOSTS[g_currentHostIndex];
        if (!host || strlen(host) == 0)
        {
            return;
        }

        if (WSUtils::isSelfHost(host))
        {
            if (LOG_VERBOSE)
            {
                Serial.println(F("[HTTP][HEALTH][SKIP] host==self-ip; ajuste WS_HOST_STR para IP do gateway"));
            }
            return;
        }

        unsigned long now = millis();
        if (now - state.lastHealthcheckAt < 15000)
        {
            return;
        }

        if (g_webSocket.isConnected())
        {
            return;
        }

        state.lastHealthcheckAt = now;

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
            Serial.println(F("[HTTP][HEALTH][ERRO] Falha em iniciar conexão"));
        }
    }

    void startConnection()
    {
        auto &state = Operation::getState();
        unsigned long now = millis();

        if (now < state.wsNextAllowedConnectAt)
        {
            if (LOG_VERBOSE)
            {
                Serial.print(F("[WS][BACKOFF] Aguardando "));
                Serial.print(state.wsNextAllowedConnectAt - now);
                Serial.println(F(" ms para nova tentativa"));
            }
            return;
        }

        // Primeira tentativa de conexão - mostrar diagnóstico
        static bool firstAttempt = true;
        if (firstAttempt)
        {
            firstAttempt = false;
            WSUtils::printNetworkDiagnostics();

            Serial.println(F("[WS] Hosts configurados:"));
            for (size_t i = 0; i < ALT_WS_HOSTS_COUNT; i++)
            {
                Serial.print(F("  ["));
                Serial.print(i);
                Serial.print(F("] "));
                Serial.println(ALT_WS_HOSTS[i] ? ALT_WS_HOSTS[i] : "(null)");
            }
            Serial.println();
        }

        const char *host = ALT_WS_HOSTS[g_currentHostIndex];
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

            WSUtils::rotateHost(ALT_WS_HOSTS, ALT_WS_HOSTS_COUNT, g_currentHostIndex, true);
            host = ALT_WS_HOSTS[g_currentHostIndex];

            if (!host || !*host || WSUtils::isSelfHost(host))
            {
                Serial.println(F("[WS][ERRO] Sem host válido diferente do IP da placa."));
                if (WS_CONNECT_ONCE)
                {
                    state.wsConnectGaveUp = true;
                    Serial.println(F("[WS][CONNECT_ONCE] Parando tentativas - configure WS_HOST_STR."));
                }
                return;
            }
        }

        // Teste rápido de conectividade antes de tentar WebSocket
        Serial.print(F("[WS] Tentativa de conexão com: "));
        Serial.println(host);

        if (!WSUtils::isHostReachable(host, WS_PORT, 2000))
        {
            Serial.println(F("[WS] Host não alcançável, rotacionando para próximo..."));
            WSUtils::rotateHost(ALT_WS_HOSTS, ALT_WS_HOSTS_COUNT, g_currentHostIndex, true);

            // Agendar próxima tentativa mais cedo para testar o próximo host
            state.wsNextAllowedConnectAt = now + 1000; // 1 segundo apenas
            return;
        }

        Serial.print(F("[WS] ✓ Conectando WebSocket a ws://"));
        Serial.print(host);
        Serial.print(":");
        Serial.print(WS_PORT);
        Serial.print(F("/ws?carId="));
        Serial.println(CAR_ID_STR);

        String path = String("/ws?carId=") + CAR_ID_STR;
        g_webSocket.begin(host, WS_PORT, path.c_str());
        g_webSocket.onEvent(onEvent);
        g_webSocket.setReconnectInterval(5000);
        g_webSocket.enableHeartbeat(30000, 10000, 2);

        state.lastWsConnectAttemptAt = millis();
        state.wsConnectAttempts = 1;
        state.wsInHandshake = true;
        state.wsHandshakeStartedAt = state.lastWsConnectAttemptAt;
    }

    void update()
    {
        g_webSocket.loop();

        auto &state = Operation::getState();

        // Envio atrasado do HELLO/STATUS se configurado
        if (state.pendingHelloSend &&
            (millis() - state.pendingHelloScheduledAt >= (unsigned long)WS_HELLO_DELAY_MS))
        {
            state.pendingHelloSend = false;
            sendHello();
            publishStatus("STOPPED");
        }

        // Se não conectado e tem host configurado
        if (!g_webSocket.isConnected() && strlen(WS_HOST_STR) != 0)
        {
            unsigned long now = millis();

            // Timeout de handshake
            if (state.wsInHandshake &&
                (now - state.wsHandshakeStartedAt > WS_HANDSHAKE_TIMEOUT_MS))
            {

                if (LOG_VERBOSE)
                {
                    Serial.println(F("[WS][TIMEOUT] Handshake excedeu limite"));
                }

                state.wsInHandshake = false;

                if (WS_CONNECT_ONCE)
                {
                    state.wsConnectGaveUp = true;
                    Serial.println(F("[WS][TIMEOUT] Modo conexão única – não haverá nova tentativa"));
                }
                else
                {
                    state.wsNextAllowedConnectAt = now + WS_BASE_RETRY_MS;
                    WSUtils::rotateHost(ALT_WS_HOSTS, ALT_WS_HOSTS_COUNT, g_currentHostIndex, true);

                    if (LOG_VERBOSE)
                    {
                        Serial.print(F("[WS][TIMEOUT] Próximo host: "));
                        Serial.println(ALT_WS_HOSTS[g_currentHostIndex] ? ALT_WS_HOSTS[g_currentHostIndex] : "(null)");
                    }
                }
            }

            // Tentar conectar quando permitido
            if (!state.wsInHandshake && now >= state.wsNextAllowedConnectAt)
            {
                if (WS_CONNECT_ONCE && (state.wsConnectAttempts >= 1 || state.wsConnectGaveUp))
                {
                    return;
                }

                if (LOG_VERBOSE)
                {
                    Serial.print(F("[WS][DEBUG] Tentando conectar. RSSI="));
                    Serial.print(Net::rssi());
                    Serial.print(F(" IP="));
                    Serial.println(Net::ip());
                }

                startConnection();
            }

            tryHttpHealthcheck();
        }
    }

} // namespace WebSocketManager