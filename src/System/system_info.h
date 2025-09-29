#pragma once

#include <Arduino.h>

namespace SystemInfo
{

    // ===== INFORMAÇÕES DO SISTEMA =====
    void showSystemInfo();
    void printMemoryInfo();
    void printNetworkInfo();
    void printHardwareInfo();
    void printProjectInfo();
    void printPeripheralsInfo();

    // ===== UTILITÁRIOS =====
    uint32_t getFreeHeap();
}