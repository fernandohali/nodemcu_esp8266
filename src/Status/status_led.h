#pragma once

#include <Arduino.h>

namespace StatusLED
{
    /**
     * @brief Inicializa o LED de status WiFi
     */
    void begin();

    /**
     * @brief Liga o LED indicando WiFi conectado
     */
    void wifiConnected();

    /**
     * @brief Desliga o LED indicando WiFi desconectado
     */
    void wifiDisconnected();

    /**
     * @brief Pisca o LED indicando tentativa de conexão
     */
    void wifiConnecting();

    /**
     * @brief Atualiza o status do LED baseado no estado do WiFi
     */
    void update();

    /**
     * @brief Retorna true se o LED está aceso
     */
    bool isOn();
}