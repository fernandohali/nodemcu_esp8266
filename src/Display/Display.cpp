#include "Display.h"
#include <time.h>

namespace Disp
{
    static TM1637Display display(TM1637_CLK, TM1637_DIO);
    static constexpr unsigned long STATUS_FLASH_TIME = 2000;
    static unsigned long statusShownAt = 0;
    static bool showingStatus = false;
    static String lastStatus = "STOP";
    static unsigned long lastClockUpdate = 0;

    void showText4(const String &txt)
    {
        uint8_t segs[4];
        auto segLetter = [](char c)
        {
            c = toupper(c);
            switch (c)
            {
            case '0':
                return 0b00111111;
            case '1':
                return 0b00000110;
            case '2':
                return 0b01011011;
            case '3':
                return 0b01001111;
            case '4':
                return 0b01100110;
            case '5':
                return 0b01101101;
            case '6':
                return 0b01111101;
            case '7':
                return 0b00000111;
            case '8':
                return 0b01111111;
            case '9':
                return 0b01101111;
            case 'R':
                return 0b01010000;
            case 'U':
                return 0b00111110;
            case 'N':
                return 0b00110111;
            case 'S':
                return 0b01101101;
            case 'T':
                return 0b01111000;
            case 'O':
                return 0b00111111;
            case 'P':
                return 0b01110011;
            case 'M':
                return 0b00110111;
            case 'A':
                return 0b01110111;
            case 'I':
                return 0b00000110;
            case ' ':
                return 0b00000000;
            case '-':
                return 0b01000000;
            default:
                return 0b00000000;
            }
        };
        for (int i = 0; i < 4; i++)
            segs[i] = segLetter(i < (int)txt.length() ? txt[i] : ' ');
        display.setSegments(segs);
    }

    void showClock()
    {
        time_t now = time(nullptr);
        if (now < 1000)
        {
            // Tempo ainda não sincronizado, mostrar "----"
            showText4("----");
            return;
        }
        
        struct tm info;
        localtime_r(&now, &info);
        int hh = info.tm_hour;
        int mm = info.tm_min;
        int value = hh * 100 + mm;
        
        // Debug no serial
        static unsigned long lastDebug = 0;
        if (millis() - lastDebug > 10000) // a cada 10 segundos
        {
            lastDebug = millis();
            Serial.print(F("[DISPLAY] Tempo: "));
            Serial.print(hh);
            Serial.print(":");
            Serial.print(mm);
            Serial.print(F(" (timestamp="));
            Serial.print(now);
            Serial.println(")");
        }
        
        display.showNumberDecEx(value, 0b01000000, true);
    }

    void begin()
    {
        Serial.print(F("[DISPLAY] Inicializando TM1637 - DIO:"));
        Serial.print(TM1637_DIO);
        Serial.print(F(" CLK:"));
        Serial.println(TM1637_CLK);
        
        display.setBrightness(0x0f);
        display.clear();
        
        // Teste inicial do display
        showText4("TEST");
        delay(1000);
        display.clear();
        
        Serial.println(F("[DISPLAY] Inicializado com sucesso"));
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
        showText4(String(text));
        showingStatus = false; // Força exibição do texto personalizado
    }

    void loop()
    {
        unsigned long ms = millis();
        if (!showingStatus && ms - lastClockUpdate >= 1000)
        {
            lastClockUpdate = ms;
            showClock();
        }
        if (showingStatus && ms - statusShownAt >= STATUS_FLASH_TIME)
        {
            showingStatus = false;
            showClock();
        }
    }
}
