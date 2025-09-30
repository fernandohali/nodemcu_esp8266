#include "Display.h"
#include <time.h>

namespace Disp
{
    static constexpr unsigned long STATUS_FLASH_TIME = 2000;
    static unsigned long statusShownAt = 0;
    static bool showingStatus = false;
    static String lastStatus = "STOP";
    static unsigned long lastClockUpdate = 0;

    // Tabela de segmentos para display de 7 segmentos (cátodo comum)
    static const uint8_t digitSegments[10] = {
        0b00111111, // 0
        0b00000110, // 1
        0b01011011, // 2
        0b01001111, // 3
        0b01100110, // 4
        0b01101101, // 5
        0b01111101, // 6
        0b00000111, // 7
        0b01111111, // 8
        0b01101111  // 9
    };

    // Tabela de caracteres para letras
    static const uint8_t charSegments[26] = {
        0b01110111, // A
        0b01111100, // b
        0b00111001, // C
        0b01011110, // d
        0b01111001, // E
        0b01110001, // F
        0b00111101, // G
        0b01110110, // H
        0b00000110, // I
        0b00001110, // J
        0b01110110, // K (mesmo que H)
        0b00111000, // L
        0b00110111, // M (aproximação)
        0b01010100, // n
        0b00111111, // O
        0b01110011, // P
        0b01100111, // q
        0b01010000, // r
        0b01101101, // S
        0b01111000, // t
        0b00111110, // U
        0b00111110, // V (mesmo que U)
        0b00111110, // W (mesmo que U)
        0b01110110, // X (mesmo que H)
        0b01101110, // y
        0b01011011  // Z (mesmo que 2)
    };

    void writeToShiftRegister(uint8_t digit1, uint8_t digit2, uint8_t digit3, uint8_t digit4, bool colonOn = false)
    {
        // Debug das informações enviadas - apenas raramente
        static unsigned long lastDebugDisplay = 0;
        if (millis() - lastDebugDisplay > 30000)
        { // Debug a cada 30 segundos
            lastDebugDisplay = millis();
            Serial.printf("[DISPLAY] -> %02X %02X %02X %02X (%s)\n",
                          digit1, digit2, digit3, digit4, colonOn ? ":" : " ");
        }

        digitalWrite(HC595_LATCH_PIN, LOW);
        delayMicroseconds(10); // Pequeno delay para estabilizar

        // Para módulos duplos 74HC595D em cascata
        // A ordem pode precisar ser ajustada dependendo da configuração do hardware

        // Primeiro 74HC595D (controla dígitos 3 e 4 - lado direito)
        // Dígito 4 (mais à direita) - com dois pontos se necessário
        uint8_t digit4_data = digit4;
        if (colonOn)
        {
            digit4_data |= 0b10000000; // Ponto decimal para dois pontos
        }
        shiftOut(HC595_DATA_PIN, HC595_CLOCK_PIN, MSBFIRST, digit4_data);

        // Dígito 3
        shiftOut(HC595_DATA_PIN, HC595_CLOCK_PIN, MSBFIRST, digit3);

        // Segundo 74HC595D (controla dígitos 1 e 2 - lado esquerdo)
        // Dígito 2 - com dois pontos no meio se necessário
        uint8_t digit2_data = digit2;
        if (colonOn)
        {
            digit2_data |= 0b10000000; // Ponto decimal para simular ":"
        }
        shiftOut(HC595_DATA_PIN, HC595_CLOCK_PIN, MSBFIRST, digit2_data);

        // Dígito 1 (mais à esquerda)
        shiftOut(HC595_DATA_PIN, HC595_CLOCK_PIN, MSBFIRST, digit1);

        delayMicroseconds(10); // Delay antes de fazer latch
        digitalWrite(HC595_LATCH_PIN, HIGH);
        delayMicroseconds(10); // Delay após latch
    }

    uint8_t getCharSegment(char c)
    {
        c = toupper(c);
        if (c >= '0' && c <= '9')
        {
            return digitSegments[c - '0'];
        }
        if (c >= 'A' && c <= 'Z')
        {
            return charSegments[c - 'A'];
        }
        switch (c)
        {
        case ' ':
            return 0b00000000;
        case '-':
            return 0b01000000;
        case '_':
            return 0b00001000;
        default:
            return 0b00000000;
        }
    }

    void showText4(const String &txt)
    {
        uint8_t seg1 = getCharSegment(txt.length() > 0 ? txt[0] : ' ');
        uint8_t seg2 = getCharSegment(txt.length() > 1 ? txt[1] : ' ');
        uint8_t seg3 = getCharSegment(txt.length() > 2 ? txt[2] : ' ');
        uint8_t seg4 = getCharSegment(txt.length() > 3 ? txt[3] : ' ');

        writeToShiftRegister(seg1, seg2, seg3, seg4, false);
    }

    void showTime(const String &timeStr)
    {
        // Para formato "MM:SS" - mostra os 4 dígitos com dois pontos
        if (timeStr.length() == 5 && timeStr[2] == ':')
        {
            uint8_t min1 = digitSegments[timeStr[0] - '0'];
            uint8_t min2 = digitSegments[timeStr[1] - '0'];
            uint8_t sec1 = digitSegments[timeStr[3] - '0'];
            uint8_t sec2 = digitSegments[timeStr[4] - '0'];

            writeToShiftRegister(min1, min2, sec1, sec2, true);
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
            // Tempo ainda não sincronizado, mostrar "----"
            showText4("----");

            // Debug para tempo não sincronizado
            static unsigned long lastNoTimeDebug = 0;
            if (millis() - lastNoTimeDebug > 5000) // a cada 5 segundos
            {
                lastNoTimeDebug = millis();
                Serial.println(F("[DISPLAY] Aguardando sincronizacao de tempo..."));
            }
            return;
        }

        struct tm info;
        localtime_r(&now, &info);
        int hh = info.tm_hour;
        int mm = info.tm_min;
        int ss = info.tm_sec;

        // Debug detalhado no serial
        static unsigned long lastDebug = 0;
        if (millis() - lastDebug > 30000) // a cada 30 segundos
        {
            lastDebug = millis();
            Serial.println(F("+===========================================+"));
            Serial.println(F("| INFORMACOES DE TEMPO                  |"));
            Serial.println(F("+-------------------------------------------+"));
            Serial.printf("| Horario atual  : %02d:%02d:%02d           |\n", hh, mm, ss);
            Serial.printf("| Timestamp Unix : %lld         |\n", (long long)now);
            Serial.printf("| Uptime sistema : %lu ms          |\n", millis());
            Serial.printf("| Memoria livre  : %d bytes        |\n", ESP.getFreeHeap());
            Serial.println(F("+===========================================+"));
            Serial.println();
        }

        // Mostra HH:MM com dois pontos
        uint8_t h1 = digitSegments[hh / 10];
        uint8_t h2 = digitSegments[hh % 10];
        uint8_t m1 = digitSegments[mm / 10];
        uint8_t m2 = digitSegments[mm % 10];

        // Pisca os dois pontos a cada segundo para indicar funcionamento
        static bool colonBlink = true;
        static unsigned long lastColonBlink = 0;
        if (millis() - lastColonBlink >= 1000)
        {
            lastColonBlink = millis();
            colonBlink = !colonBlink;
        }

        writeToShiftRegister(h1, h2, m1, m2, colonBlink);
    }

    void begin()
    {
        Serial.print(F("[DISPLAY] Inicializando 74HC595 - DATA:D"));
        Serial.print(HC595_DATA_PIN);
        Serial.print(F(" LATCH:D"));
        Serial.print(HC595_LATCH_PIN);
        Serial.print(F(" CLOCK:D"));
        Serial.println(HC595_CLOCK_PIN);

        // Configura os pinos como saída
        pinMode(HC595_DATA_PIN, OUTPUT);
        pinMode(HC595_LATCH_PIN, OUTPUT);
        pinMode(HC595_CLOCK_PIN, OUTPUT);

        // Limpa o display
        writeToShiftRegister(0, 0, 0, 0, false);

        // Teste sequencial para verificar cada dígito
        Serial.println(F("[DISPLAY] Iniciando teste sequencial..."));

        // Testa cada dígito individualmente
        Serial.println(F("[DISPLAY] Testando dígito 1 (esquerda)"));
        writeToShiftRegister(digitSegments[1], 0, 0, 0, false);
        delay(1000);

        Serial.println(F("[DISPLAY] Testando dígito 2"));
        writeToShiftRegister(0, digitSegments[2], 0, 0, false);
        delay(1000);

        Serial.println(F("[DISPLAY] Testando dígito 3"));
        writeToShiftRegister(0, 0, digitSegments[3], 0, false);
        delay(1000);

        Serial.println(F("[DISPLAY] Testando dígito 4 (direita)"));
        writeToShiftRegister(0, 0, 0, digitSegments[4], false);
        delay(1000);

        // Teste "8888" para todos os segmentos
        Serial.println(F("[DISPLAY] Testando todos os segmentos (8888)"));
        writeToShiftRegister(digitSegments[8], digitSegments[8], digitSegments[8], digitSegments[8], false);
        delay(2000);

        // Teste com dois pontos
        Serial.println(F("[DISPLAY] Testando dois pontos (12:34)"));
        writeToShiftRegister(digitSegments[1], digitSegments[2], digitSegments[3], digitSegments[4], true);
        delay(2000);

        // Teste "TEST"
        Serial.println(F("[DISPLAY] Testando texto (TEST)"));
        showText4("TEST");
        delay(2000);

        // Limpa
        Serial.println(F("[DISPLAY] Limpando display"));
        writeToShiftRegister(0, 0, 0, 0, false);

        Serial.println(F("[DISPLAY] 74HC595 inicializado com sucesso"));
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

        // Se for formato de tempo MM:SS, usa função específica
        if (str.length() == 5 && str[2] == ':')
        {
            showTime(str);
        }
        else
        {
            showText4(str);
        }

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