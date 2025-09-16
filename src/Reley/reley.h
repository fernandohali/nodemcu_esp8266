#pragma once
#include <Arduino.h>
#include "../pins.h"

namespace Relay
{
    void begin();
    void start();
    void stop();
    void maintenance();
    bool isOn();
}
