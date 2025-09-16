#include "wifi.h"

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#elif defined(ESP32)
#include <WiFi.h>
#include <ESPmDNS.h>
#endif

namespace Net
{
    static const char *g_ssid = nullptr;
    static const char *g_pass = nullptr;
    static const char *g_hostname = nullptr;
    static unsigned long lastCheck = 0;
    static bool mdnsStarted = false;

    void begin(const char *ssid, const char *pass)
    {
        g_ssid = ssid;
        g_pass = pass;
        WiFi.mode(WIFI_STA);
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
        WiFi.disconnect();
        WiFi.begin(g_ssid, g_pass);
    }

    void setupTime()
    {
        configTime(-3 * 3600, 0, "pool.ntp.org", "time.nist.gov");
    }

    bool setupMDNS(const char *hostname)
    {
#if defined(ESP8266)
        if (!isConnected())
            return false;
        if (mdnsStarted)
            return true;
        if (!MDNS.begin(hostname && *hostname ? hostname : (g_hostname ? g_hostname : "esp8266")))
        {
            return false;
        }
        mdnsStarted = true;
        return true;
#elif defined(ESP32)
        if (!isConnected())
            return false;
        if (mdnsStarted)
            return true;
        if (!MDNS.begin(hostname && *hostname ? hostname : (g_hostname ? g_hostname : "esp32")))
        {
            return false;
        }
        mdnsStarted = true;
        return true;
#else
        return false;
#endif
    }

    void loop()
    {
        ensure();
        // Atualiza serviço mDNS, se iniciado
#if defined(ESP8266)
        if (mdnsStarted)
            MDNS.update();
#endif
        // Tenta iniciar mDNS quando conectar
        if (!mdnsStarted && isConnected() && (g_hostname && *g_hostname))
        {
            setupMDNS(g_hostname);
        }
    }
}
