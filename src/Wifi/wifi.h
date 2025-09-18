#pragma once
#include <Arduino.h>

namespace Net
{
    void begin(const char *ssid, const char *pass);
    void begin(const char *ssid, const char *pass, const char *hostname);
    void loop();
    bool isConnected();
    void ensure();
    void setupTime();
    bool setupMDNS(const char *hostname);
    long rssi();
    String ip();
    const char *hostname();
    bool waitConnected(unsigned long timeoutMs);
}
