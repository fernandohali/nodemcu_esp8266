#pragma once
#include <Arduino.h>
#include "../pins.h"

namespace Disp
{
    void begin();
    void showStatus(const char *status);  // RUNNING / STOPPED / MAINTENANCE
    void showText(const char *text);      // Mostra texto customizado (ex: tempo MM:SS)
    void showTime(const String &timeStr); // Mostra tempo no formato MM:SS com dois pontos
    void loop();                          // atualiza relógio
}
