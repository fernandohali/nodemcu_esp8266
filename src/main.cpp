#include <ESP8266WiFi.h>
#include <WebSocketsClient.h>

const char* ssid     = "WiFi C1N24A";
const char* password = "35250509";

// Ajuste para o IP do seu PC (gateway Node.js) e porta
const char* WS_HOST = "192.168.0.102"; // coloque o IPv4 do seu PC
const uint16_t WS_PORT = 8081;
const char* CAR_ID = "CAR-1757510252190-ebejktj15"; // id igual ao do backend

WebSocketsClient webSocket;

const int RELAY_PIN = D1; // ajuste conforme sua fiação

// ---- Funções auxiliares ----
void publishStatus(const char* status) {
  String json = String("{\"status\":\"") + status + "\"}";
  Serial.printf("[WS] Enviando status: %s\n", json.c_str());
  webSocket.sendTXT(json);
}

void handleAction(const String& action) {
  Serial.printf("[ACTION] Recebida: %s\n", action.c_str());

  if (action == "start") {
    digitalWrite(RELAY_PIN, HIGH);
    publishStatus("RUNNING");
  } else if (action == "stop") {
    digitalWrite(RELAY_PIN, LOW);
    publishStatus("STOPPED");
  } else if (action == "emergency") {
    digitalWrite(RELAY_PIN, LOW);
    publishStatus("MAINTENANCE");
  } else {
    Serial.printf("[ACTION] Desconhecida: %s\n", action.c_str());
  }
}

// ---- Eventos WS ----
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.println("[WS] Desconectado");
      break;

    case WStype_CONNECTED:
      Serial.printf("[WS] Conectado ao servidor: %s\n", payload);
      publishStatus("STOPPED"); // envia status inicial
      break;

    case WStype_TEXT: {
      String msg;
      for (size_t i = 0; i < length; i++) msg += (char)payload[i];
      Serial.printf("[WS] Mensagem recebida: %s\n", msg.c_str());

      // Parse simples do campo "action"
      int idx = msg.indexOf("\"action\"");
      if (idx >= 0) {
        int colon = msg.indexOf(':', idx);
        int q1 = msg.indexOf('"', colon + 1);
        int q2 = msg.indexOf('"', q1 + 1);
        if (q1 >= 0 && q2 > q1) {
          String action = msg.substring(q1 + 1, q2);
          handleAction(action);
        }
      }
      break;
    }

    case WStype_PING:
      Serial.println("[WS] PING recebido");
      break;

    case WStype_PONG:
      Serial.println("[WS] PONG recebido");
      break;

    default:
      Serial.printf("[WS] Evento não tratado: %d\n", type);
      break;
  }
}

// ---- WiFi ----
void ensureWifi() {
  if (WiFi.status() == WL_CONNECTED) return;

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.printf("Conectando ao WiFi %s", ssid);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.printf("\n[WiFi] Conectado. IP local: %s\n", WiFi.localIP().toString().c_str());
}

// ---- Setup / Loop ----
void setup() {
  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  ensureWifi();

  String path = String("/ws?carId=") + CAR_ID;
  Serial.printf("[WS] Tentando conexão em ws://%s:%u%s\n", WS_HOST, WS_PORT, path.c_str());

  webSocket.begin(WS_HOST, WS_PORT, path.c_str());
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(2000);      // reconectar a cada 2s
  webSocket.enableHeartbeat(15000, 3000, 2); // ping/pong
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) ensureWifi();
  webSocket.loop();
}
