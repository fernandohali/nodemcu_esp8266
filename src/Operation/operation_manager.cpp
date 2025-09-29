#include "operation_manager.h"
#include "../Display/Display.h"
#include "../Reley/reley.h"
#include "../HC595/HC595.h"
#include "../Config/config.h"

// Estado global da operação
static OperationState g_operationState;

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
                    }
                    else
                    {
                        // Chegou em 0:00, muda para contagem progressiva
                        g_operationState.isCountingDown = false;
                        g_operationState.extraSeconds = 0;
                        Serial.println(F("[TIMER] Tempo esgotado! Iniciando contagem progressiva..."));
                    }
                }
                else
                {
                    // Contagem progressiva (tempo extra)
                    g_operationState.extraSeconds++;
                }

                updateDisplay();
            }
        }
    }

    void updateDisplay()
    {
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

                    char timeStr[8];
                    sprintf(timeStr, "%02d:%02d", displayMinutes, displaySeconds);
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

        char timeStr[8];
        sprintf(timeStr, "%02d:%02d", displayMinutes, displaySeconds);
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

        Serial.print(F("[OPERATION] Iniciada com "));
        Serial.print(minutes);
        Serial.println(F(" minutos"));

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

        Serial.println(F("[OPERATION] Parada"));
        updateDisplay();
    }

    void pause()
    {
        if (g_operationState.status == OP_ACTIVE ||
            g_operationState.status == OP_LIBERATED_TIME)
        {
            g_operationState.status = OP_PAUSED;
            Serial.println(F("[OPERATION] Pausada"));
        }
    }

    void resume()
    {
        if (g_operationState.status == OP_PAUSED)
        {
            g_operationState.status = OP_ACTIVE;
            g_operationState.lastCountUpdate = millis();
            Serial.println(F("[OPERATION] Resumida"));
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
            g_operationState.status = OP_ACTIVE;
            start(2); // 2 minutos padrão
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
        Serial.println(F("[SESSION_DATA] Processando mensagem"));

        // TODO: Implementar parser JSON mais robusto
        // Por enquanto, manter o código de parsing existente

        // ... código de parsing existente ...
    }

} // namespace Operation