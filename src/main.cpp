// ...existing code...
#include <ESP8266WiFi.h>
#include <WebSocketsClient.h>

const char* ssid     = "restic36uesb";
const char* password = "@cpdsrestic36";

// Ajuste para o IP do seu PC (backend) e porta do WS gateway
const char* WS_HOST = "192.168.0.50"; //  substitua pelo IPv4 do seu PC
const uint16_t WS_PORT = 8081;
const char* CAR_ID = "CART-001";      // mesmo id usado no backend
  
WebSocketsClient webSocket;

const int RELAY_PIN = D1; // ajuste conforme fiação

void publishStatus(const char* status) {
  // Envia status de volta para o backend
  String json = String("{\"status\":\"") + status + "\"}";
  webSocket.sendTXT(json);
}

void handleAction(const String& action) {
  if (action == "start") {
    digitalWrite(RELAY_PIN, HIGH);
    publishStatus("RUNNING");
  } else if (action == "stop") {
    digitalWrite(RELAY_PIN, LOW);
    publishStatus("STOPPED");
  } else if (action == "emergency") {
    digitalWrite(RELAY_PIN, LOW);
    publishStatus("MAINTENANCE");
  }
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.println("[WS] Desconectado");
      break;
    case WStype_CONNECTED:
      Serial.printf("[WS] Conectado a: %s\n", payload);
      // Opcional: enviar um primeiro status
      publishStatus("STOPPED");
      break;
    case WStype_TEXT: {
      String msg;
      for (size_t i = 0; i < length; i++) msg += (char)payload[i];
      Serial.printf("[WS] Mensagem: %s\n", msg.c_str());

      // Parse simples do campo "action": {"action":"start"}
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
    default:
      break;
  }
}

void ensureWifi() {
  if (WiFi.status() == WL_CONNECTED) return;
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.printf("Conectando ao WiFi %s", ssid);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.printf("\nWiFi OK. IP: %s\n", WiFi.localIP().toString().c_str());
}

void setup() {
  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  ensureWifi();

  // Conecta ao WS: caminho inclui o carId como query param
  String path = String("/ws?carId=") + CAR_ID;
  webSocket.begin(WS_HOST, WS_PORT, path.c_str());
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(2000);                // tenta reconectar a cada 2s
  webSocket.enableHeartbeat(15000, 3000, 2);           // ping/pong
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) ensureWifi();
  webSocket.loop();
}