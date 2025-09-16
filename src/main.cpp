// Novo main refatorado usando módulos
#include "pins.h"
#include "Wifi/wifi.h"
#include "Display/Display.h"
#include "Reley/reley.h"
#include <WebSocketsClient.h>

#ifndef LOOP_DELAY_MS
#define LOOP_DELAY_MS 15 // Ajuste via build_flags: -DLOOP_DELAY_MS=10
#endif

static const char *ssid = "restic36uesb";
static const char *password = "@cpdsrestic36";
static const char *WS_HOST = "192.168.1.31";
static const uint16_t WS_PORT = 8081;
static const char *CAR_ID = "CAR-1757510252190-ebejktj15";
static const char *HOSTNAME = "esp8266car"; // acessível como esp8266car.local via mDNS

WebSocketsClient webSocket;

namespace App
{
  void publishStatus(const char *status)
  {
    String json = String("{\"status\":\"") + status + "\"}";
    webSocket.sendTXT(json);
    Disp::showStatus(status);
  }

  void handleAction(const String &action)
  {
    if (action == "start")
    {
      Relay::start();
      publishStatus("RUNNING");
    }
    else if (action == "stop")
    {
      Relay::stop();
      publishStatus("STOPPED");
    }
    else if (action == "emergency")
    {
      Relay::maintenance();
      publishStatus("MAINTENANCE");
    }
  }
}

void webSocketEvent(WStype_t type, uint8_t *payload, size_t length)
{
  switch (type)
  {
  case WStype_CONNECTED:
    App::publishStatus("STOPPED");
    break;
  case WStype_TEXT:
  {
    String msg;
    msg.reserve(length);
    for (size_t i = 0; i < length; i++)
      msg += (char)payload[i];
    int idx = msg.indexOf("\"action\"");
    if (idx >= 0)
    {
      int colon = msg.indexOf(':', idx);
      int q1 = msg.indexOf('"', colon + 1);
      int q2 = msg.indexOf('"', q1 + 1);
      if (q1 >= 0 && q2 > q1)
      {
        String action = msg.substring(q1 + 1, q2);
        App::handleAction(action);
      }
    }
    break;
  }
  default:
    break;
  }
}

void setup()
{
  Serial.begin(115200);
#ifdef ESP8266
  Serial.setDebugOutput(false); // evita logs extras da stack WiFi/SDK no terminal
#endif
  Relay::begin();
  Disp::begin();
  Net::begin(ssid, password, HOSTNAME);
  Net::setupTime();
  Net::setupMDNS(HOSTNAME);

  String path = String("/ws?carId=") + CAR_ID;
  webSocket.begin(WS_HOST, WS_PORT, path.c_str());
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(2000);
  webSocket.enableHeartbeat(15000, 3000, 2);
  Disp::showStatus("STOPPED");
}

void loop()
{
  Net::loop();
  webSocket.loop();
  Disp::loop();
  // Pequeno descanso para não saturar a simulação/terminal do Wokwi
  delay(LOOP_DELAY_MS); // 10-20ms é seguro; use 5-10ms para testes de performance do WS
}
