#pragma once

#include <Arduino.h>
#include "../Operation/operation_state.h"

namespace Operation
{

    // ===== FUNÇÕES PRINCIPAIS =====
    void initialize();
    void update();

    // ===== CONTROLE DE OPERAÇÃO =====
    void start(int minutes);
    void startFromSeconds(int totalSeconds);
    void stop();
    void pause();
    void resume();
    void liberateWithoutTime();

    // ===== DISPLAY E TEMPO =====
    void updateDisplay();
    void updateTimeCounter();

    // ===== ESTADO =====
    OperationState &getState();
    const char *getStatusString();

    // ===== PROCESSAMENTO DE MENSAGENS =====
    void handleOperationMessage(const String &message);
    void handleSessionData(const String &message);
    void handleAction(const String &action);
}