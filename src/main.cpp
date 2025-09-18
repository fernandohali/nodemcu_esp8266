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

static const char *ssid = "";
static const char *password = "@";
#ifndef WS_HOST_STR
#define WS_HOST_STR "" // Ajuste via build_flags: -DWS_HOST_STR=\"192.168.1.130\"
#endif
static const uint16_t WS_PORT = 8081;
static const char *CAR_ID = ""; // único por carro, pode ser qualquer string, esse valor só vai ser gerado no site cada placa tem uma.

WebSocketsClient webSocket;
static unsigned long lastHeartbeatAt = 0;
static String g_lastStatus = "STOPPED";
static unsigned long lastInboundAt = 0; // último evento recebido do servidor

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
    json += (Net::hostname() ? Net::hostname() : "");
    json += "\",";
    json += "\"board\":\"ESP8266\"";
    json += "}";
    webSocket.sendTXT(json);
    // Log único para identificar IP da placa no Serial Monitor
    Serial.print(F("Device IP: "));
    Serial.println(Net::ip());
  }

  void printConnectionSnapshot()
  {
    String snap;
    snap.reserve(96);
    snap += "{\"carId\":\"";
    snap += CAR_ID;
    snap += "\",";
    snap += "\"online\":";
    snap += (webSocket.isConnected() ? "true" : "false");
    snap += ",";
    snap += "\"lastSeenSec\":";
    if (webSocket.isConnected() && lastInboundAt > 0)
    {
      snap += ((millis() - lastInboundAt) / 1000);
    }
    else
    {
      snap += "null";
    }
    snap += "}";
    Serial.println(snap);
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
    // Também imprime o snapshot para o Serial, como solicitado
    printConnectionSnapshot();
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
    lastInboundAt = millis();
    App::sendHello();
    App::publishStatus("STOPPED");
    App::printConnectionSnapshot();
    break;
  case WStype_DISCONNECTED:
    App::printConnectionSnapshot();
    break;
  case WStype_TEXT:
  {
    lastInboundAt = millis();
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
  Net::begin(ssid, password);
  Serial.println(F("Conectando ao WiFi..."));
  if (!Net::waitConnected(15000))
  { // espera até 15s
    Serial.println(F("Falha ao conectar no WiFi dentro do timeout."));
  }
  Net::setupTime();
  // mDNS desabilitado por preferência: usaremos apenas IP

  String path = String("/ws?carId=") + CAR_ID;
  webSocket.begin(WS_HOST_STR, WS_PORT, path.c_str());
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
