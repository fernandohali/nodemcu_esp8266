# Configuração de Múltiplas Placas ESP8266

## Como configurar cada placa com ID único

### 1. Para a placa atual (ESP-1B2185):

```ini
-DCAR_ID_STR=\"CAR-1758112436133-q5wqs3t1v\"
```

### 2. Para criar IDs para novas placas:

#### Opção A - Manual:

- Use o formato: `CAR-<timestamp>-<random>`
- Timestamp: data/hora atual em milliseconds
- Random: string aleatória de 8-10 caracteres

#### Opção B - Usando script JavaScript:

```javascript
// Gerar novo CAR_ID
const timestamp = Date.now();
const random = Math.random().toString(36).substring(2, 12);
const carId = `CAR-${timestamp}-${random}`;
console.log(`-DCAR_ID_STR="${carId}"`);
```

### 3. Exemplos de IDs únicos:

```ini
# Placa 1 (ESP-1B2185)
-DCAR_ID_STR=\"CAR-1758112436133-q5wqs3t1v\"

# Placa 2 (ESP-XXXXXX)
-DCAR_ID_STR=\"CAR-1758112436456-x8y9z1a2b3\"

# Placa 3 (ESP-YYYYYY)
-DCAR_ID_STR=\"CAR-1758112436789-c4d5e6f7g8\"
```

### 4. Configuração da rede:

- **WS_HOST_STR**: Sempre o IP do PC onde roda o servidor (`192.168.0.100`)
- **WS_PORT**: Sempre `8081`
- **Cada placa** terá seu próprio IP automático via DHCP

### 5. Passos para adicionar nova placa:

1. Clone este projeto
2. Mude o `CAR_ID_STR` no `platformio.ini`
3. Compile e faça upload na nova placa
4. A placa se conectará automaticamente ao servidor com seu ID único

### 6. Verificação no servidor:

O servidor `ws-gateway.js` mostrará:

```
[WS] Nova conexão: CAR-1758112436133-q5wqs3t1v (192.168.0.102)
[WS] Nova conexão: CAR-1758112436456-x8y9z1a2b3 (192.168.0.103)
```

### 7. Comandos para cada placa:

```bash
# Enviar comando para placa específica
curl -X POST http://localhost:8081/api/ws/connections/CAR-1758112436133-q5wqs3t1v \
  -H "Content-Type: application/json" \
  -d '{"action": "RELAY_ON"}'
```
