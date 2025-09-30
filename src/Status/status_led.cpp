#include "status_led.h"
#include "../pins.h"
#include <ESP8266WiFi.h>

namespace StatusLED
{
    static bool ledState = false;
    static unsigned long lastBlink = 0;
    static bool blinkState = false;
    static bool wasConnected = false;

    void begin()
    {
        pinMode(WIFI_LED_PIN, OUTPUT);
        digitalWrite(WIFI_LED_PIN, LOW);
        ledState = false;
        Serial.print(F("[STATUS_LED] Inicializado no pino "));
        Serial.println(WIFI_LED_PIN);
    }

    void wifiConnected()
    {
        if (!ledState)
        {
            digitalWrite(WIFI_LED_PIN, HIGH);
            ledState = true;
            Serial.println(F("[STATUS_LED] WiFi conectado - LED ligado"));
        }
    }

    void wifiDisconnected()
    {
        if (ledState)
        {
            digitalWrite(WIFI_LED_PIN, LOW);
            ledState = false;
            Serial.println(F("[STATUS_LED] WiFi desconectado - LED desligado"));
        }
    }

    void wifiConnecting()
    {
        unsigned long now = millis();
        if (now - lastBlink >= 500)
        { // Pisca a cada 500ms
            lastBlink = now;
            blinkState = !blinkState;
            digitalWrite(WIFI_LED_PIN, blinkState ? HIGH : LOW);
            ledState = blinkState;
        }
    }

    void update()
    {
        bool connected = WiFi.status() == WL_CONNECTED;

        if (connected && !wasConnected)
        {
            // WiFi acabou de conectar
            wifiConnected();
            wasConnected = true;
        }
        else if (!connected && wasConnected)
        {
            // WiFi acabou de desconectar
            wifiDisconnected();
            wasConnected = false;
        }
        else if (!connected)
        {
            // WiFi desconectado - piscar
            wifiConnecting();
        }
        // Se connected && wasConnected, mantém LED ligado (não faz nada)
    }

    bool isOn()
    {
        return ledState;
    }
}