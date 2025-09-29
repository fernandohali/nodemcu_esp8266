#pragma once

#include <Arduino.h>
#include <WebSocketsClient.h>

namespace WebSocketManager
{

    // ===== INICIALIZAÇÃO E CONTROLE =====
    void initialize();
    void update();
    void startConnection();

    // ===== EVENTOS =====
    void onEvent(WStype_t type, uint8_t *payload, size_t length);

    // ===== ENVIO DE MENSAGENS =====
    void sendHello();
    void sendHeartbeat();
    void publishStatus(const char *status);

    // ===== ESTADO DA CONEXÃO =====
    bool isConnected();
    void tryHttpHealthcheck();

    // ===== DIAGNÓSTICO =====
    void printConnectionSnapshot();
    void printDetailedSnapshot();

    // ===== ACESSO AO WEBSOCKET =====
    WebSocketsClient &getClient();
}