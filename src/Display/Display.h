#pragma once
#include <Arduino.h>
#include <TM1637Display.h>
#include "../pins.h"

namespace Disp
{
    void begin();
    void showStatus(const char *status); // RUNNING / STOPPED / MAINTENANCE
    void showText(const char *text);     // Mostra texto customizado (ex: tempo MM:SS)
    void loop();                         // atualiza relógio
}
