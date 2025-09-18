#include "wifi.h"

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#elif defined(ESP32)
#include <WiFi.h>
#endif

namespace Net
{
    static const char *g_ssid = nullptr;
    static const char *g_pass = nullptr;
    static const char *g_hostname = nullptr;
    static unsigned long lastCheck = 0;
    static bool mdnsStarted = false; // mDNS desabilitado
    static bool autoReconnect = true;

    void printStatus()
    {
#if defined(ESP8266)
        wl_status_t st = WiFi.status();
        Serial.print(F("WiFi status: "));
        Serial.print(st);
        Serial.print(F(" RSSI:"));
        Serial.print(WiFi.RSSI());
        Serial.print(F(" IP:"));
        Serial.println(WiFi.localIP());
#else
        Serial.print(F("WiFi status: "));
        Serial.print((int)WiFi.status());
        Serial.print(F(" RSSI:"));
        Serial.print(WiFi.RSSI());
        Serial.print(F(" IP:"));
        Serial.println(WiFi.localIP());
#endif
    }

    void begin(const char *ssid, const char *pass)
    {
        g_ssid = ssid;
        g_pass = pass;
        WiFi.mode(WIFI_STA);
        WiFi.setAutoConnect(true);
        WiFi.setAutoReconnect(true);
        WiFi.begin(g_ssid, g_pass);
    }

    void begin(const char *ssid, const char *pass, const char *hostname)
    {
        g_hostname = hostname;
        g_ssid = ssid;
        g_pass = pass;
        WiFi.mode(WIFI_STA);
#if defined(ESP8266)
        if (g_hostname && *g_hostname)
            WiFi.hostname(g_hostname);
#elif defined(ESP32)
        if (g_hostname && *g_hostname)
            WiFi.setHostname(g_hostname);
#endif
        WiFi.setAutoConnect(true);
        WiFi.setAutoReconnect(true);
        WiFi.begin(g_ssid, g_pass);
    }

    bool isConnected()
    {
        return WiFi.status() == WL_CONNECTED;
    }

    void ensure()
    {
        if (isConnected())
            return;
        unsigned long now = millis();
        if (now - lastCheck < 2000)
            return; // evita spam
        lastCheck = now;
        Serial.println(F("WiFi not connected, attempting reconnect..."));
#if defined(ESP8266)
        // Mostrar diagnóstico da pilha WiFi
        WiFi.printDiag(Serial);
#endif
        if (autoReconnect)
        {
            WiFi.disconnect();
            WiFi.begin(g_ssid, g_pass);
        }
    }

    void setupTime()
    {
        configTime(-3 * 3600, 0, "pool.ntp.org", "time.nist.gov");
    }

    bool setupMDNS(const char *hostname)
    {
        // mDNS intencionalmente não utilizado
        return false;
    }

    void loop()
    {
        ensure();
        // mDNS desabilitado: nada a fazer aqui
    }

    bool waitConnected(unsigned long timeoutMs)
    {
        unsigned long start = millis();
        while (millis() - start < timeoutMs)
        {
            if (isConnected())
            {
                printStatus();
                return true;
            }
            delay(100);
        }
        printStatus();
        return false;
    }

    long rssi()
    {
        if (!isConnected())
            return 0;
        return WiFi.RSSI();
    }

    String ip()
    {
        if (!isConnected())
            return String("");
#if defined(ESP8266)
        return WiFi.localIP().toString();
#else
        return WiFi.localIP().toString();
#endif
    }

    const char *hostname()
    {
        return g_hostname;
    }
}
