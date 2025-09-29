#pragma once

#include <Arduino.h>

namespace WSUtils
{
    // Retorna true se 'host' for igual ao IP atual da placa
    bool isSelfHost(const char *host);

    // Testa se um host está alcançável via TCP
    bool isHostReachable(const char *host, uint16_t port, uint32_t timeout_ms = 3000);

    // Testa o endpoint de health HTTP do servidor
    bool testHttpHealth(const char *host, uint16_t port);

    // Rotaciona o índice 'currentIndex' dentro do array 'hosts' com teste de conectividade
    void rotateHost(const char **hosts, size_t count, size_t &currentIndex, bool avoidSelf = true);

    // Imprime informações de diagnóstico de rede
    void printNetworkDiagnostics();
}
