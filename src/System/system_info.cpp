#include "system_info.h"
#include "../Wifi/wifi.h"
#include "../Config/config.h"
#include "../pins.h"
#include "../HC595/HC595.h"
#include <ESP8266WiFi.h>

namespace SystemInfo
{

    uint32_t getFreeHeap()
    {
#if defined(ESP8266) || defined(ESP32)
        return ESP.getFreeHeap();
#else
        return 0;
#endif
    }

    void printNetworkInfo()
    {
        Serial.println(F("--- WIFI ---"));
        Serial.print(F("IP: "));
        Serial.println(Net::ip());
        Serial.print(F("RSSI: "));
        Serial.print(Net::rssi());
        Serial.println(F(" dBm"));
        Serial.print(F("MAC: "));
        Serial.println(WiFi.macAddress());
        Serial.print(F("Gateway: "));
        Serial.println(WiFi.gatewayIP());
        Serial.print(F("Subnet: "));
        Serial.println(WiFi.subnetMask());
        Serial.print(F("DNS1: "));
        Serial.println(WiFi.dnsIP(0));
        Serial.print(F("DNS2: "));
        Serial.println(WiFi.dnsIP(1));
    }

    void printHardwareInfo()
    {
        Serial.println(F("--- HARDWARE ---"));
        Serial.print(F("Chip ID: "));
        Serial.println(ESP.getChipId(), HEX);
        Serial.print(F("Free Heap: "));
        Serial.print(getFreeHeap());
        Serial.println(F(" bytes"));
        Serial.print(F("Flash Size: "));
        Serial.print(ESP.getFlashChipSize());
        Serial.println(F(" bytes"));
        Serial.print(F("CPU Freq: "));
        Serial.print(ESP.getCpuFreqMHz());
        Serial.println(F(" MHz"));
    }

    void printProjectInfo()
    {
        Serial.println(F("--- PROJETO ---"));
        Serial.print(F("Car ID: "));
        Serial.println(CAR_ID_STR);
        Serial.print(F("WS Host: "));
        Serial.println(WS_HOST_STR);
        Serial.print(F("WS Port: "));
        Serial.println(WS_PORT);
        Serial.print(F("Connect Once: "));
        Serial.println(WS_CONNECT_ONCE ? F("SIM") : F("NÃO"));
    }

    void printPeripheralsInfo()
    {
        Serial.println(F("--- PERIFÉRICOS ---"));
        Serial.print(F("Relay Pin: "));
        Serial.println(RELAY_PIN);
        Serial.print(F("Display 74HC595 DATA: "));
        Serial.println(HC595_DATA_PIN);
        Serial.print(F("Display 74HC595 LATCH: "));
        Serial.println(HC595_LATCH_PIN);

        Serial.println(F("--- 74HC595 SHIFT REGISTER ---"));
        Serial.print(F("Data Pin (D2): "));
        Serial.println(HC595_DATA_PIN);
        Serial.print(F("Latch Pin (D3): "));
        Serial.println(HC595_LATCH_PIN);
        Serial.print(F("Clock Pin (D4): "));
        Serial.println(HC595_CLOCK_PIN);
        Serial.print(F("Estado atual: 0b"));
        Serial.println(HC595::getByte(), BIN);
    }

    void printMemoryInfo()
    {
        Serial.print(F("Free Heap: "));
        Serial.print(getFreeHeap());
        Serial.println(F(" bytes"));
    }

    void showSystemInfo()
    {
        Serial.println(F("\n======= INFORMAÇÕES DO SISTEMA ======="));

        printNetworkInfo();
        printHardwareInfo();
        printProjectInfo();
        printPeripheralsInfo();

        Serial.println(F("======================================\n"));
    }

} // namespace SystemInfo