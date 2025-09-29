#pragma once

#include <Arduino.h>

namespace SerialCommands
{

    // ===== PROCESSAMENTO DE COMANDOS =====
    void processCommands();
    void handleCommand(char command);
    void showHelp();
}