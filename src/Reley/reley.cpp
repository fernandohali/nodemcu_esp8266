#include "reley.h"

namespace Relay
{
    static bool state = false;

    void begin()
    {
        pinMode(RELAY_PIN, OUTPUT);
        digitalWrite(RELAY_PIN, LOW);
        state = false;
    }

    void start()
    {
        digitalWrite(RELAY_PIN, HIGH);
        state = true;
    }

    void stop()
    {
        digitalWrite(RELAY_PIN, LOW);
        state = false;
    }

    void maintenance()
    {
        digitalWrite(RELAY_PIN, LOW);
        state = false;
    }

    bool isOn()
    {
        return state;
    }
}
