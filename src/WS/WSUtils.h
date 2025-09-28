#pragma once

#include <Arduino.h>

namespace WSUtils
{
    // Retorna true se 'host' for igual ao IP atual da placa
    bool isSelfHost(const char *host);

    // Rotaciona o índice 'currentIndex' dentro do array 'hosts' (tamanho 'count').
    // Se avoidSelf=true, evita selecionar um host igual ao IP da própria placa.
    void rotateHost(const char **hosts, size_t count, size_t &currentIndex, bool avoidSelf = true);
}
