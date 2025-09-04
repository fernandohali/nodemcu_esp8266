#include <Arduino.h>
#include <ESP8266WiFi.h>

const char* ssid = "SUA_REDE_WIFI";
const char* password = "SENHA_WIFI";

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);

  Serial.println("Conectando ao WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi conectado!");
  Serial.print("IP local: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  // Aqui você pode rodar outras lógicas
}
