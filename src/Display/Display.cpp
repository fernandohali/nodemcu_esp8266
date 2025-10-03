#include "Display.h"
#include <time.h>

namespace Disp
{
    static constexpr unsigned long STATUS_FLASH_TIME = 2000;
    static unsigned long statusShownAt = 0;
    static bool showingStatus = false;
    static String lastStatus = "STOP";
    static unsigned long lastClockUpdate = 0;

    // CÁTODO COMUM - padrões da biblioteca DIYables
    static const uint8_t digitSegments[10] = {
        0b11000000, // 0
        0b11111001, // 1
        0b10100100, // 2
        0b10110000, // 3
        0b10011001, // 4
        0b10010010, // 5
        0b10000010, // 6
        0b11111000, // 7
        0b10000000, // 8
        0b10010000  // 9
    };

    // Valores dos 4 dígitos e pontos decimais (estado atual)
    static uint8_t _digit_values[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    static uint8_t _digit_dots = 0x00;

    // Função shift adaptada da biblioteca DIYables
    void shift(uint8_t value)
    {
        for (uint8_t i = 8; i >= 1; i--)
        {
            if (value & 0x80)
                digitalWrite(HC595_DATA_PIN, HIGH);
            else
                digitalWrite(HC595_DATA_PIN, LOW);

            value <<= 1;
            digitalWrite(HC595_CLOCK_PIN, LOW);
            digitalWrite(HC595_CLOCK_PIN, HIGH);
        }
    }

    // Loop de multiplexação - DEVE SER CHAMADO CONTINUAMENTE!
    void multiplex()
    {
        for (int i = 0; i < 4; i++)
        {
            // Seletor de dígito: 0x08, 0x04, 0x02, 0x01
            int digit_selector = 0x08 >> i;
            uint8_t value = _digit_values[i];

            // Adiciona ponto decimal se necessário
            if (_digit_dots & (1 << i))
                value &= 0x7F; // Liga o bit 7 (ponto decimal)

            shift(value);           // Primeiro: padrão dos segmentos
            shift(digit_selector);  // Segundo: qual dígito acender

            // Faz o latch
            digitalWrite(HC595_LATCH_PIN, LOW);
            digitalWrite(HC595_LATCH_PIN, HIGH);
        }
    }

    void setDigit(int pos, uint8_t pattern)
    {
        if (pos >= 0 && pos < 4)
            _digit_values[pos] = pattern;
    }

    void setDot(int pos, bool on)
    {
        if (pos >= 0 && pos < 4)
        {
            if (on)
                _digit_dots |= (1 << pos);
            else
                _digit_dots &= ~(1 << pos);
        }
    }

    void clearDisplay()
    {
        for (int i = 0; i < 4; i++)
            _digit_values[i] = 0xFF;
        _digit_dots = 0x00;
    }

    uint8_t getCharSegment(char c)
    {
        c = toupper(c);
        if (c >= '0' && c <= '9')
            return digitSegments[c - '0'];
        
        // Caracteres especiais
        if (c == '-')
            return 0b10111111; // traço
        if (c == ' ')
            return 0xFF; // apagado
        
        return 0xFF;
    }

    void showText4(const String &txt)
    {
        for (int i = 0; i < 4; i++)
        {
            if (i < txt.length())
                setDigit(i, getCharSegment(txt[i]));
            else
                setDigit(i, 0xFF);
        }
        setDot(0, false);
        setDot(1, false);
        setDot(2, false);
        setDot(3, false);
    }

    void showTime(const String &timeStr)
    {
        // Para formato "MM:SS" - mostra os 4 dígitos com dois pontos
        if (timeStr.length() == 5 && timeStr[2] == ':')
        {
            setDigit(0, digitSegments[timeStr[0] - '0']);
            setDigit(1, digitSegments[timeStr[1] - '0']);
            setDigit(2, digitSegments[timeStr[3] - '0']);
            setDigit(3, digitSegments[timeStr[4] - '0']);
            
            // Dois pontos usando pontos decimais
            setDot(1, true);  // Ponto superior
            setDot(2, true);  // Ponto inferior
            return;
        }

        // Fallback para texto normal
        showText4(timeStr);
    }

    void showClock()
    {
        time_t now = time(nullptr);
        if (now < 1000)
        {
            showText4("----");
            return;
        }

        struct tm info;
        localtime_r(&now, &info);
        int hh = info.tm_hour;
        int mm = info.tm_min;

        char timeStr[6];
        snprintf(timeStr, sizeof(timeStr), "%02d:%02d", hh, mm);
        showTime(timeStr);
    }

    void begin()
    {
        Serial.println(F("[DISPLAY] Inicializando 74HC595 MULTIPLEXADO"));
        Serial.printf("DATA:%d LATCH:%d CLOCK:%d\n", HC595_DATA_PIN, HC595_LATCH_PIN, HC595_CLOCK_PIN);

        pinMode(HC595_DATA_PIN, OUTPUT);
        pinMode(HC595_LATCH_PIN, OUTPUT);
        pinMode(HC595_CLOCK_PIN, OUTPUT);

        clearDisplay();

        // Teste "8888"
        Serial.println(F("[DISPLAY] Teste: 8888"));
        for (int i = 0; i < 4; i++)
            setDigit(i, digitSegments[8]);
        
        for (int t = 0; t < 2000; t++)
        {
            multiplex();
            delay(1);
        }

        // Teste "1234"
        Serial.println(F("[DISPLAY] Teste: 1234"));
        setDigit(0, digitSegments[1]);
        setDigit(1, digitSegments[2]);
        setDigit(2, digitSegments[3]);
        setDigit(3, digitSegments[4]);
        setDot(1, true);
        setDot(2, true);
        
        for (int t = 0; t < 2000; t++)
        {
            multiplex();
            delay(1);
        }

        clearDisplay();
        Serial.println(F("[DISPLAY] Inicializado com sucesso!"));
    }

    void showStatus(const char *status)
    {
        if (strcmp(status, "RUNNING") == 0)
            lastStatus = "RUN";
        else if (strcmp(status, "STOPPED") == 0)
            lastStatus = "STOP";
        else if (strcmp(status, "MAINTENANCE") == 0)
            lastStatus = "MAIN";
        else
            lastStatus = "----";

        showText4(lastStatus);
        showingStatus = true;
        statusShownAt = millis();
    }

    void showText(const char *text)
    {
        String str = String(text);

        if (str.length() == 5 && str[2] == ':')
            showTime(str);
        else
            showText4(str);

        showingStatus = false;
    }

    void loop()
    {
        // MULTIPLEXAÇÃO - atualiza o display continuamente
        multiplex();

        // Atualiza o relógio a cada 1 segundo
        static unsigned long lastUpdate = 0;
        if (millis() - lastUpdate >= 1000)
        {
            lastUpdate = millis();
            
            if (!showingStatus)
                showClock();
        }

        // Remove status após tempo definido
        if (showingStatus && millis() - statusShownAt >= STATUS_FLASH_TIME)
        {
            showingStatus = false;
            showClock();
        }
    }
}
