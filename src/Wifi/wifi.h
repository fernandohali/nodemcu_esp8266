#pragma once
#include <Arduino.h>

namespace Net
{
    void begin(const char *ssid, const char *pass);
    void begin(const char *ssid, const char *pass, const char *hostname);
    void configureStaticIp(const char *ip, const char *gateway, const char *mask, const char *dns1 = nullptr, const char *dns2 = nullptr);
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
