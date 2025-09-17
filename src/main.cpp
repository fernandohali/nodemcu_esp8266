// Novo main refatorado usando módulos
#include "pins.h"
#include "Wifi/wifi.h"
#include "Display/Display.h"
#include "Reley/reley.h"
#include <WebSocketsClient.h>

#ifndef LOOP_DELAY_MS
#define LOOP_DELAY_MS 200 // Ajuste via build_flags: -DLOOP_DELAY_MS=10
#endif

#ifndef HEARTBEAT_MS
#define HEARTBEAT_MS 10000 // 10s: ajuste com -DHEARTBEAT_MS=5000 p/ testes
#endif

static const char *ssid = "restic36uesb";
static const char *password = "@cpdsrestic36";
static const char *WS_HOST = "192.168.1.31";
static const uint16_t WS_PORT = 8081;
static const char *CAR_ID = "CAR-1758112436133-q5wqs3t1v"; // único por carro, pode ser qualquer string, esse valor só vai ser gerado no site cada placa tem uma.
static const char *HOSTNAME = "esp8266car"; // acessível como esp8266car.local via mDNS

WebSocketsClient webSocket;
static unsigned long lastHeartbeatAt = 0;
static String g_lastStatus = "STOPPED";

namespace App
{
  void publishStatus(const char *status)
  {
    g_lastStatus = status;
    String json;
    json.reserve(128);
    json += "{\"type\":\"status\",";
    json += "\"carId\":\"";
    json += CAR_ID;
    json += "\",";
    json += "\"status\":\"";
    json += status;
    json += "\",";
    json += "\"relayOn\":";
    json += (Relay::isOn() ? "true" : "false");
    json += "}";
    webSocket.sendTXT(json);
    Disp::showStatus(status);
  }

  void sendHello()
  {
    String json;
    json.reserve(256);
    json += "{\"type\":\"hello\",";
    json += "\"carId\":\"";
    json += CAR_ID;
    json += "\",";
    json += "\"ip\":\"";
    json += Net::ip();
    json += "\",";
    json += "\"hostname\":\"";
    json += (Net::hostname() ? Net::hostname() : HOSTNAME);
    json += "\",";
    json += "\"mdns\":\"";
    json += (Net::hostname() ? String(Net::hostname()) + ".local" : String(HOSTNAME) + ".local");
    json += "\",";
    json += "\"board\":\"ESP8266\"";
    json += "}";
    webSocket.sendTXT(json);
  }

  void sendHeartbeat()
  {
    unsigned long now = millis();
    if (now - lastHeartbeatAt < HEARTBEAT_MS)
      return;
    lastHeartbeatAt = now;
    String json;
    json.reserve(256);
    json += "{\"type\":\"heartbeat\",";
    json += "\"carId\":\"";
    json += CAR_ID;
    json += "\",";
    json += "\"status\":\"";
    json += g_lastStatus;
    json += "\",";
    json += "\"relayOn\":";
    json += (Relay::isOn() ? "true" : "false");
    json += ",";
    json += "\"rssi\":";
    json += Net::rssi();
    json += ",";
    json += "\"ip\":\"";
    json += Net::ip();
    json += "\",";
    json += "\"uptimeSec\":";
    json += (now / 1000);
    json += "}";
    webSocket.sendTXT(json);
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
    App::sendHello();
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
  App::sendHeartbeat();
  Disp::loop();
  // Pequeno descanso para não saturar a simulação/terminal do Wokwi
  delay(LOOP_DELAY_MS); // 10-20ms é seguro; use 5-10ms para testes de performance do WS
}
