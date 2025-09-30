#include "operation_manager.h"
#include "../Display/Display.h"
#include "../Reley/reley.h"
#include "../HC595/HC595.h"
#include "../Config/config.h"

// Estado global da operação
OperationState g_operationState;

namespace Operation
{

    void initialize()
    {
        g_operationState = OperationState();
        g_operationState.lastCountUpdate = millis();
    }

    OperationState &getState()
    {
        return g_operationState;
    }

    const char *getStatusString()
    {
        return statusToString(g_operationState.status);
    }

    void updateTimeCounter()
    {
        unsigned long now = millis();

        // Atualiza a cada 1 segundo
        if (now - g_operationState.lastCountUpdate >= 1000)
        {
            g_operationState.lastCountUpdate = now;

            // Só conta se estiver em modo ativo ou liberado com tempo
            if (g_operationState.status == OP_ACTIVE ||
                g_operationState.status == OP_LIBERATED_TIME)
            {

                if (g_operationState.isCountingDown)
                {
                    // Contagem regressiva
                    if (g_operationState.remainingSeconds > 0)
                    {
                        g_operationState.remainingSeconds--;

                        // Log apenas a cada minuto ou nos últimos 10 segundos
                        if ((g_operationState.remainingSeconds % 60 == 0) ||
                            (g_operationState.remainingSeconds <= 10 && g_operationState.remainingSeconds > 0))
                        {
                            int mins = g_operationState.remainingSeconds / 60;
                            int secs = g_operationState.remainingSeconds % 60;
                            Serial.printf("⏳ Restam: %02d:%02d\n", mins, secs);
                        }
                    }
                    else
                    {
                        // Chegou em 0:00, muda para contagem progressiva
                        g_operationState.isCountingDown = false;
                        g_operationState.extraSeconds = 0;
                        Serial.println(F("\n⏰ TEMPO ESGOTADO!"));
                        Serial.println(F("🔄 Iniciando contagem progressiva..."));
                        Serial.println(F("⚠️  Sistema em tempo extra!\n"));
                    }
                }
                else
                {
                    // Contagem progressiva (tempo extra)
                    g_operationState.extraSeconds++;

                    // Log de tempo extra a cada minuto
                    if (g_operationState.extraSeconds % 60 == 0)
                    {
                        int mins = g_operationState.extraSeconds / 60;
                        int secs = g_operationState.extraSeconds % 60;
                        Serial.printf("⚠️  Tempo extra: +%02d:%02d\n", mins, secs);
                    }
                }

                updateDisplay();
            }
        }
    }

    void updateDisplay()
    {
        // Debug do status atual
        static unsigned long lastStatusDebug = 0;
        if (millis() - lastStatusDebug > 5000)
        { // Debug a cada 5 segundos
            lastStatusDebug = millis();
            Serial.printf("[DISPLAY_STATUS] Status: %s | Remaining: %d | Extra: %d | Countdown: %s\n",
                          getStatusString(),
                          g_operationState.remainingSeconds,
                          g_operationState.extraSeconds,
                          g_operationState.isCountingDown ? "YES" : "NO");
        }

        if (g_operationState.status == OP_STOPPED)
        {
            Disp::showText("--:--");
            return;
        }

        if (g_operationState.status == OP_LIBERATED_FREE)
        {
            Disp::showText("FREE");
            return;
        }

        if (g_operationState.status == OP_PAUSED)
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
                    if (g_operationState.isCountingDown)
                    {
                        displayMinutes = g_operationState.remainingSeconds / 60;
                        displaySeconds = g_operationState.remainingSeconds % 60;
                    }
                    else
                    {
                        displayMinutes = g_operationState.extraSeconds / 60;
                        displaySeconds = g_operationState.extraSeconds % 60;
                    }

                    char timeStr[16]; // Buffer maior para evitar overflow
                    snprintf(timeStr, sizeof(timeStr), "%02d:%02d", displayMinutes, displaySeconds);
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
        // Se não há tempo definido ainda (aguardando servidor), mostra "----"
        if (g_operationState.isCountingDown && g_operationState.remainingSeconds == 0 &&
            g_operationState.extraSeconds == 0)
        {
            Disp::showText("----");
            return;
        }

        int displayMinutes, displaySeconds;

        if (g_operationState.isCountingDown)
        {
            displayMinutes = g_operationState.remainingSeconds / 60;
            displaySeconds = g_operationState.remainingSeconds % 60;
        }
        else
        {
            displayMinutes = g_operationState.extraSeconds / 60;
            displaySeconds = g_operationState.extraSeconds % 60;
        }

        char timeStr[16]; // Buffer maior para evitar overflow
        snprintf(timeStr, sizeof(timeStr), "%02d:%02d", displayMinutes, displaySeconds);
        Disp::showText(timeStr);
    }

    void start(int minutes)
    {
        g_operationState.initialMinutes = minutes;
        g_operationState.remainingSeconds = minutes * 60;
        g_operationState.extraSeconds = 0;
        g_operationState.isCountingDown = true;
        g_operationState.lastCountUpdate = millis();

        // Liga relay
        if (!g_operationState.relayState)
        {
            Relay::start();
            g_operationState.relayState = true;
        }

        Serial.println(F("\n🚀 === OPERACAO INICIADA ==="));
        Serial.printf("⏰ Duracao: %d min (%d seg)\n", minutes, minutes * 60);
        Serial.printf("📊 Status: %s\n", getStatusString());
        Serial.printf("🔄 Modo: %s\n", g_operationState.isCountingDown ? "Regressiva" : "Progressiva");
        Serial.println(F("=============================\n"));

        updateDisplay();
    }

    void stop()
    {
        g_operationState.status = OP_STOPPED;
        g_operationState.remainingSeconds = 0;
        g_operationState.extraSeconds = 0;
        g_operationState.lastCountUpdate = 0;

        // Desliga relay
        if (g_operationState.relayState)
        {
            Relay::stop();
            g_operationState.relayState = false;
        }

        Serial.println(F("\n🛑 === OPERACAO PARADA ==="));
        Serial.printf("⏱️  Tempo total: %lu seg\n",
                      (millis() - (g_operationState.lastCountUpdate > 0 ? g_operationState.lastCountUpdate : millis())) / 1000);
        Serial.printf("📊 Status final: %s\n", getStatusString());
        Serial.println(F("==========================\n"));
        updateDisplay();
    }

    void pause()
    {
        if (g_operationState.status == OP_ACTIVE ||
            g_operationState.status == OP_LIBERATED_TIME)
        {
            g_operationState.status = OP_PAUSED;
            int remainingMins = g_operationState.remainingSeconds / 60;
            int remainingSecs = g_operationState.remainingSeconds % 60;
            Serial.printf("⏸️  PAUSADO - Restam: %02d:%02d\n", remainingMins, remainingSecs);
        }
    }

    void resume()
    {
        if (g_operationState.status == OP_PAUSED)
        {
            g_operationState.status = OP_ACTIVE;
            g_operationState.lastCountUpdate = millis();
            int remainingMins = g_operationState.remainingSeconds / 60;
            int remainingSecs = g_operationState.remainingSeconds % 60;
            Serial.printf("▶️  RESUMIDO - Continuando: %02d:%02d\n", remainingMins, remainingSecs);
        }
    }

    void liberateWithoutTime()
    {
        g_operationState.status = OP_LIBERATED_FREE;
        g_operationState.remainingSeconds = 0;
        g_operationState.extraSeconds = 0;

        // Liga relay
        if (!g_operationState.relayState)
        {
            Relay::start();
            g_operationState.relayState = true;
        }

        Serial.println(F("[OPERATION] Liberada sem tempo"));
        updateDisplay();
    }

    void update()
    {
        updateTimeCounter();
    }

    // ===== PROCESSAMENTO DE MENSAGENS =====

    void handleAction(const String &action)
    {
        // Comandos simples
        if (action == "start")
        {
            Serial.println(F("📥 COMANDO START RECEBIDO"));
            Serial.println(F("🚀 Ativando operacao - aguardando tempo do servidor..."));
            g_operationState.status = OP_ACTIVE; // Ativa para ligar relay
            g_operationState.lastCountUpdate = millis();

            // Configura valores temporários até receber dados do servidor
            g_operationState.remainingSeconds = 0; // Será sobrescrito pelo servidor
            g_operationState.extraSeconds = 0;
            g_operationState.isCountingDown = true;

            // Liga relay imediatamente
            if (!g_operationState.relayState)
            {
                Relay::start();
                g_operationState.relayState = true;
                Serial.println(F("⚡ Relay ligado (aguardando tempo do servidor)"));
            }

            // Mostra "aguardando" no display
            Disp::showText("----");
            Serial.println(F("📺 Display: Aguardando dados do servidor..."));
        }
        else if (action == "stop")
        {
            stop();
        }
        else if (action == "pause")
        {
            pause();
        }
        else if (action == "resume")
        {
            resume();
        }
        else if (action == "liberate_free")
        {
            liberateWithoutTime();
        }
        else if (action == "emergency")
        {
            stop();
        }
        // Comandos HC595
        else if (action.startsWith("hc595_pin_"))
        {
            int pinIndex = action.substring(10, 11).toInt();
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
            uint8_t value = action.substring(11).toInt();
            HC595::setByte(value);
            HC595::update();
            Serial.print(F("[HC595] Byte definido: 0b"));
            Serial.println(value, BIN);
        }
    }

    void handleOperationMessage(const String &message)
    {
        // Implementação simplificada - usar apenas para comandos básicos
        Serial.print(F("[OPERATION] Processando mensagem de operação: "));
        Serial.println(message.substring(0, 50) + "...");

        // TODO: Implementar parser JSON completo
    }

    void handleSessionData(const String &message)
    {
        Serial.println(F("\n📋 Recebido dados da sessao..."));
        // Debug completo apenas se necessário
        static bool showFullDebug = true;
        if (showFullDebug)
        {
            Serial.printf("Dados: %s...\n", message.substring(0, 50).c_str());
            showFullDebug = false; // Mostra apenas uma vez
        }

        // Parser JSON melhorado - procura session_data primeiro
        int sessionDataIndex = message.indexOf("\"session_data\":");
        if (sessionDataIndex >= 0)
        {
            // Se encontrou session_data, procura dentro dessa seção
            int startBrace = message.indexOf("{", sessionDataIndex);
            if (startBrace >= 0)
            {
                // Procura campos dentro de session_data
                String sessionDataStr = message.substring(startBrace);

                // Tenta múltiplos campos de tempo
                int timeValue = 0;
                bool found = false;

                // 1. Tenta "minutes":
                int minutesIdx = sessionDataStr.indexOf("\"minutes\":");
                if (minutesIdx >= 0)
                {
                    int colonIdx = sessionDataStr.indexOf(":", minutesIdx);
                    if (colonIdx >= 0)
                    {
                        String numStr = "";
                        int i = colonIdx + 1;
                        while ((unsigned int)i < sessionDataStr.length() && (sessionDataStr.charAt(i) == ' ' || sessionDataStr.charAt(i) == '\t'))
                            i++;
                        while ((unsigned int)i < sessionDataStr.length() && isDigit(sessionDataStr.charAt(i)))
                        {
                            numStr += sessionDataStr.charAt(i);
                            i++;
                        }
                        if (numStr.length() > 0)
                        {
                            timeValue = numStr.toInt();
                            found = true;
                            Serial.printf("✅ Encontrou 'minutes': %d\n", timeValue);
                        }
                    }
                }

                // 2. Se não encontrou, tenta "duration":
                if (!found)
                {
                    int durationIdx = sessionDataStr.indexOf("\"duration\":");
                    if (durationIdx >= 0)
                    {
                        int colonIdx = sessionDataStr.indexOf(":", durationIdx);
                        if (colonIdx >= 0)
                        {
                            String numStr = "";
                            int i = colonIdx + 1;
                            while ((unsigned int)i < sessionDataStr.length() && (sessionDataStr.charAt(i) == ' ' || sessionDataStr.charAt(i) == '\t'))
                                i++;
                            while ((unsigned int)i < sessionDataStr.length() && isDigit(sessionDataStr.charAt(i)))
                            {
                                numStr += sessionDataStr.charAt(i);
                                i++;
                            }
                            if (numStr.length() > 0)
                            {
                                int seconds = numStr.toInt();
                                timeValue = seconds / 60; // Convert to minutes
                                found = true;
                                Serial.printf("✅ Encontrou 'duration': %d segundos = %d minutos\n", seconds, timeValue);
                            }
                        }
                    }
                }

                // 3. Se não encontrou, tenta "initialMinutes":
                if (!found)
                {
                    int initialIdx = sessionDataStr.indexOf("\"initialMinutes\":");
                    if (initialIdx >= 0)
                    {
                        int colonIdx = sessionDataStr.indexOf(":", initialIdx);
                        if (colonIdx >= 0)
                        {
                            String numStr = "";
                            int i = colonIdx + 1;
                            while ((unsigned int)i < sessionDataStr.length() && (sessionDataStr.charAt(i) == ' ' || sessionDataStr.charAt(i) == '\t'))
                                i++;
                            while ((unsigned int)i < sessionDataStr.length() && isDigit(sessionDataStr.charAt(i)))
                            {
                                numStr += sessionDataStr.charAt(i);
                                i++;
                            }
                            if (numStr.length() > 0)
                            {
                                timeValue = numStr.toInt();
                                found = true;
                                Serial.printf("✅ Encontrou 'initialMinutes': %d\n", timeValue);
                            }
                        }
                    }
                }

                if (found && timeValue > 0)
                {
                    Serial.println(F("✅ === SESSAO DO SERVIDOR RECEBIDA ==="));
                    Serial.printf("⏰ TEMPO CONFIGURADO: %d minutos\n", timeValue);
                    Serial.printf("📊 Iniciando operacao com %d min (%d seg)\n", timeValue, timeValue * 60);
                    Serial.printf("🔄 Sobrescrevendo operacao local anterior\n");
                    Serial.println(F("========================================"));

                    // Inicia a operação com o tempo correto
                    start(timeValue);
                    return;
                }
            }
        }

        // Fallback: Parser simples para compatibilidade
        int durationIndex = message.indexOf("\"duration\":");
        if (durationIndex == -1)
        {
            durationIndex = message.indexOf("\"minutes\":");
        }
        if (durationIndex == -1)
        {
            durationIndex = message.indexOf("\"initialMinutes\":");
        }
        if (durationIndex == -1)
        {
            durationIndex = message.indexOf("\"remainingTime\":");
        }

        if (durationIndex >= 0)
        {
            // Encontra o valor após os dois pontos
            int colonIndex = message.indexOf(":", durationIndex);
            if (colonIndex >= 0)
            {
                // Extrai o número
                String numberStr = "";
                int i = colonIndex + 1;

                // Pula espaços
                while ((unsigned int)i < message.length() && (message.charAt(i) == ' ' || message.charAt(i) == '\t'))
                {
                    i++;
                }

                // Extrai dígitos
                while ((unsigned int)i < message.length() && isDigit(message.charAt(i)))
                {
                    numberStr += message.charAt(i);
                    i++;
                }

                if (numberStr.length() > 0)
                {
                    int value = numberStr.toInt();
                    int minutes = value;

                    // Se encontrou remainingTime, valor está em segundos - converter para minutos
                    if (message.indexOf("\"remainingTime\":") >= 0 && value > 60)
                    {
                        minutes = value / 60; // Converte segundos para minutos
                        Serial.printf("🔄 Convertendo %d segundos para %d minutos\n", value, minutes);
                    }

                    if (minutes > 0)
                    {
                        Serial.println(F("✅ === SESSAO DO SERVIDOR RECEBIDA ==="));
                        Serial.printf("⏰ TEMPO CONFIGURADO: %d minutos\n", minutes);
                        Serial.printf("📊 Iniciando operacao com %d min (%d seg)\n", minutes, minutes * 60);
                        Serial.printf("🔄 Sobrescrevendo operacao local anterior\n");
                        Serial.println(F("========================================"));

                        // Inicia a operação com o tempo correto
                        start(minutes);
                        return;
                    }
                }
            }
        }

        // Se não conseguiu extrair, procura por outras variações
        int timeIndex = message.indexOf("\"time\":");
        if (timeIndex >= 0)
        {
            int colonIndex = message.indexOf(":", timeIndex);
            if (colonIndex >= 0)
            {
                String timeValue = "";
                int i = colonIndex + 1;
                while ((unsigned int)i < message.length() && message.charAt(i) != ',' && message.charAt(i) != '}')
                {
                    if (message.charAt(i) != ' ' && message.charAt(i) != '"')
                    {
                        timeValue += message.charAt(i);
                    }
                    i++;
                }

                int timeMinutes = timeValue.toInt();
                if (timeMinutes > 0)
                {
                    Serial.printf("⏰ TEMPO ALTERNATIVO: %d minutos\n", timeMinutes);
                    start(timeMinutes);
                    return;
                }
            }
        }

        Serial.println(F("❌ Nenhum tempo valido encontrado na mensagem"));
        Serial.printf("📨 MSG COMPLETA: %s\n", message.c_str());
        Serial.printf("📏 Tamanho: %d bytes\n", message.length());

        // Debug adicional das variações procuradas
        Serial.printf("🔍 Procurou 'duration': %s\n", message.indexOf("\"duration\":") >= 0 ? "ENCONTRADO" : "NAO ENCONTRADO");
        Serial.printf("🔍 Procurou 'minutes': %s\n", message.indexOf("\"minutes\":") >= 0 ? "ENCONTRADO" : "NAO ENCONTRADO");
        Serial.printf("🔍 Procurou 'initialMinutes': %s\n", message.indexOf("\"initialMinutes\":") >= 0 ? "ENCONTRADO" : "NAO ENCONTRADO");
        Serial.printf("🔍 Procurou 'remainingTime': %s\n", message.indexOf("\"remainingTime\":") >= 0 ? "ENCONTRADO" : "NAO ENCONTRADO");
        Serial.printf("🔍 Procurou 'time': %s\n", message.indexOf("\"time\":") >= 0 ? "ENCONTRADO" : "NAO ENCONTRADO");

        // Verifica se tem session_data
        if (message.indexOf("session_data") >= 0)
        {
            Serial.println("📋 Mensagem contem 'session_data'");
            if (message.indexOf("null") >= 0)
            {
                Serial.println("⚠️  session_data eh NULL - sem sessao ativa no servidor");
            }
        }

        Serial.println(F("=== FIM DEBUG SESSAO ==="));
    }

} // namespace Operation