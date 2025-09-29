#include "serial_commands.h"
#include "../WebSocket/websocket_manager.h"

namespace SerialCommands
{

    void showHelp()
    {
        Serial.println(F("Comandos disponíveis:"));
        Serial.println(F("  i = Snapshot detalhado"));
        Serial.println(F("  j = Snapshot JSON"));
        Serial.println(F("  h = Esta ajuda"));
    }

    void handleCommand(char command)
    {
        switch (command)
        {
        case 'i':
            WebSocketManager::printDetailedSnapshot();
            break;

        case 'j':
            WebSocketManager::printConnectionSnapshot();
            break;

        case 'h':
            showHelp();
            break;

        default:
            Serial.print(F("Comando desconhecido: "));
            Serial.println(command);
            Serial.println(F("Digite 'h' para ajuda"));
            break;
        }
    }

    void processCommands()
    {
        if (Serial.available())
        {
            char command = Serial.read();
            handleCommand(command);
        }
    }

} // namespace SerialCommands