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
        // Log apenas mudanças significativas (a cada minuto)
        static unsigned long lastMinuteLog = 0;
        static int lastLoggedMinute = -1;
        int currentMinute = g_operationState.isCountingDown ? g_operationState.remainingSeconds / 60 : g_operationState.extraSeconds / 60;
        
        if (currentMinute != lastLoggedMinute && millis() - lastMinuteLog > 30000)
        {
            lastMinuteLog = millis();
            lastLoggedMinute = currentMinute;
            int secs = g_operationState.isCountingDown ? g_operationState.remainingSeconds % 60 : g_operationState.extraSeconds % 60;
            Serial.printf("⏱️  %s: %02d:%02d\n", 
                          g_operationState.isCountingDown ? "Restam" : "Extra",
                          currentMinute, secs);
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

                    // Formato MM:SS para display pausado - dois primeiros dígitos são minutos, últimos são segundos
                    char timeStr[16];
                    snprintf(timeStr, sizeof(timeStr), "%02d%02d", displayMinutes, displaySeconds);
                    Disp::showTime(timeStr);
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

        // Formato MM:SS para display - dois primeiros dígitos são minutos, últimos são segundos
        char displayStr[16];
        snprintf(displayStr, sizeof(displayStr), "%02d%02d", displayMinutes, displaySeconds);
        Disp::showTime(displayStr);
    }

    void start(int minutes)
    {
        startFromSeconds(minutes * 60);
    }

    void startFromSeconds(int totalSeconds)
    {
        if (totalSeconds <= 0)
        {
            Serial.println(F("⚠️  Tempo invalido recebido - mantendo operacao parada"));
            g_operationState.initialMinutes = 0;
            g_operationState.initialSeconds = 0;
            g_operationState.remainingSeconds = 0;
            g_operationState.extraSeconds = 0;
            g_operationState.isCountingDown = true;
            updateDisplay();
            return;
        }

        g_operationState.initialSeconds = totalSeconds;
        g_operationState.initialMinutes = totalSeconds / 60;
        g_operationState.remainingSeconds = totalSeconds;
        g_operationState.extraSeconds = 0;
        g_operationState.isCountingDown = true;
        g_operationState.lastCountUpdate = millis();

        // Liga relay
        if (!g_operationState.relayState)
        {
            Relay::start();
            g_operationState.relayState = true;
        }

        int displayMinutes = totalSeconds / 60;
        int displaySeconds = totalSeconds % 60;

        Serial.println(F("\n🚀 === OPERACAO INICIADA ==="));
        Serial.printf("⏰ Duracao: %d min %02d seg (%d seg)\n", displayMinutes, displaySeconds, totalSeconds);
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
        Serial.println(F("\n� === DADOS DO SERVIDOR RECEBIDOS ==="));

        auto readNumberAfter = [&](const String &source, int anchorIndex) -> int {
            if (anchorIndex < 0)
            {
                return -1;
            }

            int colonIdx = source.indexOf(':', anchorIndex);
            if (colonIdx < 0)
            {
                return -1;
            }

            String numStr = "";
            int i = colonIdx + 1;
            while ((unsigned int)i < source.length() && (source.charAt(i) == ' ' || source.charAt(i) == '\t'))
            {
                i++;
            }
            while ((unsigned int)i < source.length() && isDigit(source.charAt(i)))
            {
                numStr += source.charAt(i);
                i++;
            }

            if (numStr.length() == 0)
            {
                return -1;
            }

            return numStr.toInt();
        };

        // Parser JSON melhorado - procura session_data primeiro
        int sessionDataIndex = message.indexOf("\"session_data\":");
        if (sessionDataIndex >= 0)
        {
            int startBrace = message.indexOf("{", sessionDataIndex);
            if (startBrace >= 0)
            {
                String sessionDataStr = message.substring(startBrace);

                int parsedSeconds = -1;
                int parsedMinutes = -1;

                int remainingTimeIdx = sessionDataStr.indexOf("\"remainingTime\":");
                if (remainingTimeIdx >= 0)
                {
                    int totalSecondsIdx = sessionDataStr.indexOf("\"total_seconds\":", remainingTimeIdx);
                    int seconds = readNumberAfter(sessionDataStr, totalSecondsIdx);
                    if (seconds >= 0)
                    {
                        parsedSeconds = seconds;
                        parsedMinutes = seconds / 60;
                        Serial.printf("⏰ Tempo configurado no servidor: %02d:%02d (%d segundos)\n",
                                      parsedMinutes, seconds % 60, seconds);
                    }
                }

                int minutesIdx = sessionDataStr.indexOf("\"minutes\":");
                if (minutesIdx >= 0)
                {
                    int minutes = readNumberAfter(sessionDataStr, minutesIdx);
                    if (minutes >= 0)
                    {
                        parsedMinutes = minutes;
                        if (parsedSeconds < 0)
                        {
                            parsedSeconds = minutes * 60;
                        }
                    }
                }

                int durationIdx = sessionDataStr.indexOf("\"duration\":");
                if (durationIdx >= 0)
                {
                    int seconds = readNumberAfter(sessionDataStr, durationIdx);
                    if (seconds >= 0)
                    {
                        parsedSeconds = seconds;
                        parsedMinutes = seconds / 60;
                    }
                }

                int initialIdx = sessionDataStr.indexOf("\"initialMinutes\":");
                if (initialIdx >= 0)
                {
                    int minutes = readNumberAfter(sessionDataStr, initialIdx);
                    if (minutes >= 0)
                    {
                        parsedMinutes = minutes;
                        if (parsedSeconds < 0)
                        {
                            parsedSeconds = minutes * 60;
                        }
                    }
                }

                if (parsedSeconds > 0)
                {
                    Serial.printf("✅ Sessão iniciada: %02d:%02d\n", parsedSeconds / 60, parsedSeconds % 60);
                    Serial.println(F("========================================\n"));
                    
                    startFromSeconds(parsedSeconds);
                    return;
                }

                if (parsedMinutes > 0)
                {
                    Serial.printf("✅ Sessão iniciada: %02d:00\n", parsedMinutes);
                    Serial.println(F("========================================\n"));

                    start(parsedMinutes);
                    return;
                }
            }
        }

        if (message.indexOf("session_data") >= 0 && message.indexOf("null") >= 0)
        {
            Serial.println(F("⚠️  Nenhuma sessão ativa no servidor"));
        }
        else
        {
            Serial.println(F("❌ Dados de tempo não encontrados na mensagem"));
        }
        Serial.println(F("========================================\n"));
    }

} // namespace Operation