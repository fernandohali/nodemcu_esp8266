#!/usr/bin/env node

/**
 * Servidor de descoberta automática para ESP8266
 * Este servidor roda em múltiplos IPs para facilitar a descoberta
 */

const WebSocket = require("ws");
const http = require("http");
const os = require("os");

// Configurações
const WS_PORT = 8081;
const HTTP_PORT = 8080;

// Função para obter todos os IPs da máquina
function getLocalIPs() {
  const interfaces = os.networkInterfaces();
  const ips = [];

  for (const name of Object.keys(interfaces)) {
    for (const interface of interfaces[name]) {
      // Pular loopback e interfaces não IPv4
      if (interface.family === "IPv4" && !interface.internal) {
        ips.push(interface.address);
      }
    }
  }

  return ips;
}

// Criar servidor HTTP para health check
function createHealthServer(ip) {
  const server = http.createServer((req, res) => {
    if (req.url === "/api/ws/health") {
      res.writeHead(200, {
        "Content-Type": "application/json",
        "Access-Control-Allow-Origin": "*",
      });
      res.end(
        JSON.stringify({
          status: "ok",
          server: "discovery-server",
          ip: ip,
          timestamp: new Date().toISOString(),
          uptime: process.uptime(),
        })
      );
    } else {
      res.writeHead(404);
      res.end("Not Found");
    }
  });

  return server;
}

// Criar servidor WebSocket
function createWebSocketServer(server) {
  const wss = new WebSocket.Server({ server });

  wss.on("connection", (ws, req) => {
    const url = new URL(req.url, `http://${req.headers.host}`);
    const carId = url.searchParams.get("carId") || "UNKNOWN";

    console.log(
      `🚗 [${new Date().toLocaleTimeString()}] ESP8266 conectado: ${carId}`
    );
    console.log(`   📍 IP Cliente: ${req.socket.remoteAddress}`);
    console.log(`   🌐 Servidor IP: ${req.socket.localAddress}`);

    // Enviar mensagem de boas-vindas
    ws.send(
      JSON.stringify({
        type: "welcome",
        message: "Servidor de descoberta conectado!",
        serverIp: req.socket.localAddress,
        timestamp: new Date().toISOString(),
      })
    );

    // Responder a mensagens do ESP8266
    ws.on("message", (data) => {
      try {
        const message = data.toString();
        console.log(`📨 [${carId}]: ${message}`);

        // Responder a HELLO
        if (message.startsWith("HELLO")) {
          ws.send(
            JSON.stringify({
              type: "hello_response",
              carId: carId,
              status: "connected",
              serverIp: req.socket.localAddress,
            })
          );
        }

        // Responder a JSON
        if (message.startsWith("{")) {
          const msg = JSON.parse(message);

          if (msg.type === "heartbeat") {
            // Não responder heartbeat para evitar spam
            return;
          }

          // Resposta genérica
          ws.send(
            JSON.stringify({
              type: "response",
              original_type: msg.type,
              carId: msg.carId,
              timestamp: new Date().toISOString(),
            })
          );
        }
      } catch (err) {
        console.error(
          `❌ Erro ao processar mensagem de ${carId}:`,
          err.message
        );
      }
    });

    ws.on("close", () => {
      console.log(
        `👋 [${new Date().toLocaleTimeString()}] ESP8266 desconectado: ${carId}`
      );
    });

    ws.on("error", (err) => {
      console.error(`❌ Erro WebSocket ${carId}:`, err.message);
    });
  });

  return wss;
}

// Função principal
async function main() {
  const ips = getLocalIPs();

  console.log("🚀 Iniciando Servidor de Descoberta ESP8266");
  console.log("==========================================");
  console.log(`📅 Data/Hora: ${new Date().toLocaleString()}`);
  console.log(`🔧 Porta WebSocket: ${WS_PORT}`);
  console.log(`🔧 Porta HTTP: ${HTTP_PORT}`);
  console.log(`🌐 IPs encontrados: ${ips.join(", ")}`);
  console.log("==========================================\n");

  const servers = [];

  for (const ip of ips) {
    try {
      // Criar servidor HTTP
      const httpServer = createHealthServer(ip);

      // Bind no IP específico
      httpServer.listen(WS_PORT, ip, () => {
        console.log(`✅ Servidor rodando em ${ip}:${WS_PORT}`);
        console.log(
          `   🔗 WebSocket: ws://${ip}:${WS_PORT}/ws?carId=SEU_CAR_ID`
        );
        console.log(`   💚 Health: http://${ip}:${WS_PORT}/api/ws/health\n`);
      });

      // Criar servidor WebSocket no mesmo servidor HTTP
      const wss = createWebSocketServer(httpServer);

      servers.push({ httpServer, wss, ip });
    } catch (err) {
      console.error(`❌ Erro ao iniciar servidor em ${ip}:`, err.message);
    }
  }

  if (servers.length === 0) {
    console.error("❌ Nenhum servidor foi iniciado com sucesso!");
    process.exit(1);
  }

  console.log(`🎉 ${servers.length} servidor(es) iniciado(s) com sucesso!`);
  console.log("\n📋 Para testar:");
  console.log(`   • Configure o ESP8266 com qualquer um dos IPs acima`);
  console.log(`   • O ESP8266 tentará conectar automaticamente`);
  console.log(`   • Use Ctrl+C para parar os servidores\n`);

  // Graceful shutdown
  process.on("SIGINT", () => {
    console.log("\n🛑 Parando servidores...");

    servers.forEach(({ httpServer, ip }) => {
      httpServer.close(() => {
        console.log(`✅ Servidor ${ip} parado`);
      });
    });

    setTimeout(() => {
      console.log("👋 Até logo!");
      process.exit(0);
    }, 1000);
  });
}

// Executar
if (require.main === module) {
  main().catch(console.error);
}
