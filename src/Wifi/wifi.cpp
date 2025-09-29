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
    static bool staticConfigured = false;
    static IPAddress staticIp, staticGw, staticMask, staticDns1, staticDns2;

    void configureStaticIp(const char *ip, const char *gateway, const char *mask, const char *dns1, const char *dns2)
    {
        if (ip && gateway && mask)
        {
            staticIp.fromString(ip);
            staticGw.fromString(gateway);
            staticMask.fromString(mask);
            if (dns1 && *dns1)
                staticDns1.fromString(dns1);
            if (dns2 && *dns2)
                staticDns2.fromString(dns2);
            staticConfigured = true;
        }
    }

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
#if defined(ESP8266)
        WiFi.setSleepMode(WIFI_NONE_SLEEP); // desabilita power-save que pode causar latência e quedas
#endif
        WiFi.setAutoConnect(true);
        WiFi.setAutoReconnect(true);
        if (staticConfigured)
        {
            if (staticDns1)
            {
                WiFi.config(staticIp, staticGw, staticMask, staticDns1, staticDns2);
            }
            else
            {
                WiFi.config(staticIp, staticGw, staticMask);
            }
        }
        WiFi.begin(g_ssid, g_pass);
    }

    void begin(const char *ssid, const char *pass, const char *hostname)
    {
        g_hostname = hostname;
        g_ssid = ssid;
        g_pass = pass;
        WiFi.mode(WIFI_STA);
#if defined(ESP8266)
        WiFi.setSleepMode(WIFI_NONE_SLEEP);
#endif
#if defined(ESP8266)
        if (g_hostname && *g_hostname)
            WiFi.hostname(g_hostname);
#elif defined(ESP32)
        if (g_hostname && *g_hostname)
            WiFi.setHostname(g_hostname);
#endif
        WiFi.setAutoConnect(true);
        WiFi.setAutoReconnect(true);
        if (staticConfigured)
        {
            if (staticDns1)
            {
                WiFi.config(staticIp, staticGw, staticMask, staticDns1, staticDns2);
            }
            else
            {
                WiFi.config(staticIp, staticGw, staticMask);
            }
        }
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
            if (staticConfigured)
            {
                if (staticDns1)
                {
                    WiFi.config(staticIp, staticGw, staticMask, staticDns1, staticDns2);
                }
                else
                {
                    WiFi.config(staticIp, staticGw, staticMask);
                }
            }
            WiFi.begin(g_ssid, g_pass);
        }
    }

    void setupTime()
    {
        Serial.println(F("[NTP] Configurando sincronização de tempo..."));
        configTime(-3 * 3600, 0, "pool.ntp.org", "time.nist.gov");

        Serial.print(F("[NTP] Aguardando sincronização"));
        time_t now = time(nullptr);
        int attempts = 0;
        while (now < 8 * 3600 * 2 && attempts < 15) // Aguarda até ter um timestamp válido
        {
            delay(500);
            Serial.print(".");
            now = time(nullptr);
            attempts++;
        }

        if (now >= 8 * 3600 * 2)
        {
            Serial.println(F("\n[NTP] Sincronização concluída!"));
            Serial.print(F("[NTP] Timestamp atual: "));
            Serial.println(now);

            struct tm timeinfo;
            localtime_r(&now, &timeinfo);
            Serial.print(F("[NTP] Hora local: "));
            Serial.print(timeinfo.tm_hour);
            Serial.print(":");
            Serial.print(timeinfo.tm_min);
            Serial.print(":");
            Serial.println(timeinfo.tm_sec);
        }
        else
        {
            Serial.println(F("\n[NTP] ERRO: Falha na sincronização!"));
        }
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
