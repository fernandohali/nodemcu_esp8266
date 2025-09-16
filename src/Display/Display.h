#pragma once
#include <Arduino.h>
#include <TM1637Display.h>
#include "../pins.h"

namespace Disp
{
    void begin();
    void showStatus(const char *status); // RUNNING / STOPPED / MAINTENANCE
    void loop();                         // atualiza relógio
}
