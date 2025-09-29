#include "WSUtils.h"
#include "../Wifi/wifi.h" // para Net::ip()
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <ESP8266WiFi.h>

namespace WSUtils
{

    bool isSelfHost(const char *host)
    {
        if (!host || !*host)
            return false;
        String self = Net::ip();
        return self.length() > 0 && self.equals(host);
    }

    bool isHostReachable(const char *host, uint16_t port, uint32_t timeout_ms)
    {
        if (!host || !*host)
        {
            return false;
        }

        // Ignorar localhost e loopback se não estivermos na mesma máquina
        if (strcmp(host, "localhost") == 0 || strcmp(host, "127.0.0.1") == 0)
        {
            Serial.println(F("[WSUtils] Ignorando host localhost/127.0.0.1"));
            return false;
        }

        Serial.print(F("[WSUtils] Testando conectividade com "));
        Serial.print(host);
        Serial.print(":");
        Serial.print(port);
        Serial.print(F("... "));

        WiFiClient client;
        bool connected = client.connect(host, port);

        if (connected)
        {
            Serial.println(F("OK"));
            client.stop();
            return true;
        }
        else
        {
            Serial.println(F("FALHA"));
            return false;
        }
    }

    bool testHttpHealth(const char *host, uint16_t port)
    {
        if (!host || !*host)
        {
            return false;
        }

        // Ignorar localhost
        if (strcmp(host, "localhost") == 0 || strcmp(host, "127.0.0.1") == 0)
        {
            return false;
        }

        WiFiClient client;
        HTTPClient http;
        String url = String("http://") + host + ":" + port + "/api/ws/health";

        Serial.print(F("[WSUtils] Testando health endpoint: "));
        Serial.print(url);
        Serial.print(F("... "));

        bool result = false;
        if (http.begin(client, url))
        {
            http.setTimeout(3000); // 3 segundos timeout
            int code = http.GET();
            if (code == 200)
            {
                Serial.println(F("OK"));
                result = true;
            }
            else if (code > 0)
            {
                Serial.print(F("HTTP "));
                Serial.println(code);
            }
            else
            {
                Serial.println(F("ERRO"));
            }
            http.end();
        }
        else
        {
            Serial.println(F("FALHA"));
        }

        return result;
    }

    void rotateHost(const char **hosts, size_t count, size_t &currentIndex, bool avoidSelf)
    {
        if (count <= 1)
            return;

        Serial.println(F("[WSUtils] Buscando próximo host válido..."));

        size_t start = currentIndex;
        bool foundReachable = false;

        // Primeira passagem: procurar hosts alcançáveis
        for (size_t i = 0; i < count && !foundReachable; i++)
        {
            size_t testIndex = (start + 1 + i) % count;
            const char *h = hosts[testIndex];

            if (!h || !*h)
            {
                continue;
            }

            if (avoidSelf && isSelfHost(h))
            {
                Serial.print(F("[WSUtils] Pulando host próprio: "));
                Serial.println(h);
                continue;
            }

            Serial.print(F("[WSUtils] Testando host: "));
            Serial.println(h);

            // Teste rápido de conectividade TCP
            if (isHostReachable(h, 8081, 2000))
            {
                currentIndex = testIndex;
                foundReachable = true;
                Serial.print(F("[WSUtils] ✓ Host selecionado: "));
                Serial.println(h);
                return;
            }
        }

        // Se não encontrou host alcançável, usar rotação simples
        if (!foundReachable)
        {
            Serial.println(F("[WSUtils] Nenhum host alcançável encontrado, usando rotação simples"));
            do
            {
                currentIndex = (currentIndex + 1) % count;
                const char *h = hosts[currentIndex];
                if (h && *h)
                {
                    if (!avoidSelf || !isSelfHost(h))
                    {
                        Serial.print(F("[WSUtils] → Host selecionado por rotação: "));
                        Serial.println(h);
                        break;
                    }
                }
            } while (currentIndex != start);
        }
    }

    void printNetworkDiagnostics()
    {
        Serial.println(F("\n===== DIAGNÓSTICO DE REDE ====="));
        Serial.print(F("IP local: "));
        Serial.println(Net::ip());
        Serial.print(F("Gateway: "));
        Serial.println(WiFi.gatewayIP());
        Serial.print(F("DNS 1: "));
        Serial.println(WiFi.dnsIP(0));
        Serial.print(F("DNS 2: "));
        Serial.println(WiFi.dnsIP(1));
        Serial.print(F("RSSI: "));
        Serial.print(Net::rssi());
        Serial.println(F(" dBm"));
        Serial.println(F("===============================\n"));
    }
}
