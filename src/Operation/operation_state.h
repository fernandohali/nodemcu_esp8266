#pragma once

// ===== ESTADOS DA OPERAÇÃO =====
enum OperationStatus
{
    OP_STOPPED = 0,    // Parado - relay desligado
    OP_ACTIVE,         // Ativo - contando regressivamente
    OP_PAUSED,         // Pausado - relay ligado, contador congelado
    OP_LIBERATED_TIME, // Liberado com tempo - conta normalmente
    OP_LIBERATED_FREE  // Liberado sem tempo - relay ligado, sem contador
};

// ===== ESTRUTURA DE CONTROLE DE OPERAÇÃO =====
struct OperationState
{
    OperationStatus status = OP_STOPPED;
    String sessionCarId = "";
    int initialMinutes = 0;
    int initialSeconds = 0;
    int remainingSeconds = 0;
    int extraSeconds = 0;
    bool isCountingDown = true;
    unsigned long lastCountUpdate = 0;
    bool relayState = false;

    // Métrica do WebSocket
    unsigned long lastInboundAt = 0;
    unsigned long lastHealthcheckAt = 0;
    unsigned long lastWsConnectAttemptAt = 0;
    uint32_t wsConnectAttempts = 0;
    bool wsConnectGaveUp = false;

    // Sessão WebSocket
    unsigned long currentSessionStartedAt = 0;
    uint32_t sessionSentFrames = 0;
    uint32_t sessionRecvFrames = 0;

    // Controle de reconexão
    unsigned long wsNextAllowedConnectAt = 0;
    bool wsInHandshake = false;
    unsigned long wsHandshakeStartedAt = 0;

    // Hello pendente
    bool pendingHelloSend = false;
    unsigned long pendingHelloScheduledAt = 0;

    String lastStatus = "STOPPED";
};

// Converte OperationStatus para string
inline const char *statusToString(OperationStatus status)
{
    switch (status)
    {
    case OP_STOPPED:
        return "STOPPED";
    case OP_ACTIVE:
        return "ACTIVE";
    case OP_PAUSED:
        return "PAUSED";
    case OP_LIBERATED_TIME:
        return "LIBERATED_TIME";
    case OP_LIBERATED_FREE:
        return "LIBERATED_FREE";
    default:
        return "UNKNOWN";
    }
}