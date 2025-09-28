# nodemcu_esp8266

Firmware ESP8266 (NodeMCU) para cliente WebSocket de telemetria e controle de relé, com modos de diagnóstico extensivos.

## Build / PlatformIO

Edite `platformio.ini` e adicione `build_flags` conforme necessário.

Exemplo mínimo (substitua valores reais):

```ini
build_flags =
  -DWS_HOST_STR="192.168.0.102"
  -DCAR_ID_STR="CAR-1758112436133-q5wqs3t1v"
  -DLOG_VERBOSE
  -DWS_DISABLE_FALLBACK
```

## Flags Principais

Identidade / Rede:

- `WS_HOST_STR` (obrigatório) IP ou hostname do gateway WS.
- `CAR_ID_STR` Identificador único do carro.
- `STATIC_IP_ADDR`, `STATIC_IP_GW`, `STATIC_IP_MASK` (e opcionais `STATIC_IP_DNS1`, `STATIC_IP_DNS2`) para IP fixo.

Reconnect / Handshake:

- `WS_BASE_RETRY_MS` (default 3000) base do backoff.
- `WS_MAX_RETRY_MS` (default 30000) máximo do backoff exponencial.
- `WS_HANDSHAKE_TIMEOUT_MS` (default 8000) aborta tentativa se não conecta nesse prazo.
- `WS_HELLO_DELAY_MS` atrasa envio de hello/status após CONNECTED.
- `WS_DISABLE_FALLBACK` desativa hosts alternativos embutidos.

Heartbeat:

- (Padrão ativo) `App::sendHeartbeat()` envia JSON; defina `WS_DISABLE_HEARTBEAT` (se macro adicionada) para testar sem heartbeats.

Logs / Telemetria:

- `LOG_VERBOSE` habilita logs detalhados (recomendado para diagnóstico).
- `LOG_HEARTBEAT_JSON` imprime JSON de heartbeat no Serial.
- `LOG_STATUS_JSON` imprime JSON de status no Serial.
- `LOG_COLOR` ativa cores ANSI (se monitor suportar).

Modos HELLO (compatibilidade):

- `WS_HELLO_SIMPLE` envia apenas texto: `HELLO <carId>`.
- `WS_HELLO_COMPAT` envia JSON padrão e depois linha: `HELLO-CAR <carId>`.
- (Nenhuma macro) envia só JSON `{ "type":"hello", ... }`.

Se ambas forem definidas, `WS_HELLO_SIMPLE` prevalece.

## Sequência de Mensagens

1. CONNECTED → (opcional atraso) envio de HELLO.
2. Envio de `status` inicial (STOPPED).
3. Heartbeats periódicos (contêm RSSI, heap, uptime, relay, status).
4. Servidor pode enviar ações JSON com campo `action`: `start|stop|emergency`.

## Métricas de Sessão

Ao desconectar, loga: duração (ms), frames enviados/recebidos, tempo desde último inbound.

## Troubleshooting Rápido

### Passo 1: Confirmar IP do gateway

Windows: `ipconfig` (anote IPv4 da interface LAN)

### Passo 2: Testar porta 8081 aberta

`powershell -c "Test-NetConnection -ComputerName 192.168.0.102 -Port 8081"`

### Passo 3: Health HTTP manual (PC)

`curl http://192.168.0.102:8081/api/ws/health`

### Passo 4: Testar WebSocket manual (PC)

Instale wscat: `npm i -g wscat`

`wscat -c ws://192.168.0.102:8081/ws?carId=CAR-TEST`

Se wscat conecta mas ESP não: focar em firmware / handshake / headers.

Se wscat também cai (code 1006): problema no gateway ou rede (firewall, proxy, rota).

### Passo 5: Ativar diagnóstico no firmware

```ini
build_flags =
  -DWS_HOST_STR="192.168.0.102"
  -DCAR_ID_STR="CAR-XXXX"
  -DLOG_VERBOSE
  -DWS_DISABLE_FALLBACK
  -DWS_BASE_RETRY_MS=5000
  -DWS_MAX_RETRY_MS=60000
  -DWS_HANDSHAKE_TIMEOUT_MS=8000
  -DWS_HELLO_DELAY_MS=1500
```

### Passo 6: Modo compatível de HELLO

Adicionar `-DWS_HELLO_COMPAT` ou `-DWS_HELLO_SIMPLE` se suspeitar que servidor não parseia JSON inicial.

### Passo 7: Validar Backoff e Timeouts

Observar logs `[WS][BACKOFF]` (intervalos dobrando) e `[WS][TIMEOUT]` (handshake travado).

## Possíveis Causas do Code=1006

| Categoria                     | Sintoma                           | Ação                                                       |
| ----------------------------- | --------------------------------- | ---------------------------------------------------------- |
| Path incorreto                | Servidor não reconhece `/ws`      | Confirmar rota real e ajustar path no código se necessário |
| Firewall                      | Health HTTP falha                 | Liberar porta 8081 / app Node no firewall                  |
| Headers exigidos              | Servidor espera `Origin`          | Adicionar suporte extra headers (futuro)                   |
| Queda antes do primeiro frame | Nenhum HELLO recebido no servidor | Capturar upgrade logs no backend                           |
| Crash parsing                 | Reset de uptime                   | Observar uptime_s reiniciando                              |

## Extensões Futuras (planejadas)

- Macro para path customizado (`WS_PATH_STR`).
- Headers extras (`WS_EXTRA_ORIGIN`).
- Endpoint HTTP adicional de ping.

## Licença

Projeto interno / uso experimental.
