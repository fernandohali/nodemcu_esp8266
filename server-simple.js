#!/usr/bin/env node
/**
 * Servidor WebSocket simples para ESP8266
 * Funciona com PC em dual-network (Ethernet + WiFi)
 */

const WebSocket = require("ws");
const http = require("http");

const PORT = 8081;

console.log("🚀 Servidor WebSocket para ESP8266");

// Criar servidor HTTP
const server = http.createServer((req, res) => {
  if (req.url === "/health" || req.url === "/api/ws/health") {
    res.writeHead(200, { "Content-Type": "application/json" });
    res.end(
      JSON.stringify({
        status: "ok",
        server: "ESP8266-Server",
        port: PORT,
        timestamp: new Date().toISOString(),
      })
    );
  } else {
    res.writeHead(404);
    res.end("Not Found");
  }
});

// Criar servidor WebSocket
const wss = new WebSocket.Server({
  server: server,
  path: "/ws",
});

let connectionCount = 0;

wss.on("connection", (ws, req) => {
  connectionCount++;

  // Extrair carId da URL
  const url = new URL(req.url, `http://${req.headers.host}`);
  const carId = url.searchParams.get("carId") || "UNKNOWN";
  const clientIP = req.socket.remoteAddress;

  console.log(`✅ [${new Date().toLocaleTimeString()}] ESP8266 Conectado:`);
  console.log(`   🚗 Car ID: ${carId}`);
  console.log(`   📡 IP: ${clientIP}`);
  console.log(`   🔗 Conexão #${connectionCount}`);

  // Mensagem de boas-vindas
  ws.send(
    JSON.stringify({
      type: "hello",
      carId: carId,
      timeout: 30000,
      timestamp: Date.now(),
    })
  );

  // Heartbeat
  const heartbeat = setInterval(() => {
    if (ws.readyState === WebSocket.OPEN) {
      ws.ping();
    }
  }, 30000);

  // Escutar mensagens
  ws.on("message", (data) => {
    try {
      const message = JSON.parse(data.toString());
      console.log(`📨 [${carId}] Recebido:`, message.type || "data");

      // Responder baseado no tipo
      if (message.type === "hello") {
        ws.send(
          JSON.stringify({
            type: "welcome",
            message: "Conectado ao servidor",
            timestamp: Date.now(),
          })
        );
      }
    } catch (error) {
      console.log(`📨 [${carId}] Raw:`, data.toString());
    }
  });

  // Pong (resposta ao ping)
  ws.on("pong", () => {
    console.log(`💓 [${carId}] Pong recebido`);
  });

  // Desconexão
  ws.on("close", (code, reason) => {
    clearInterval(heartbeat);
    console.log(
      `❌ [${new Date().toLocaleTimeString()}] ESP8266 Desconectado:`
    );
    console.log(`   🚗 Car ID: ${carId}`);
    console.log(`   📄 Código: ${code}`);
  });

  ws.on("error", (error) => {
    console.log(`⚠️  [${carId}] Erro:`, error.message);
  });
});

// Iniciar servidor em todas as interfaces
server.listen(PORT, "0.0.0.0", () => {
  console.log(`🌐 Servidor escutando na porta ${PORT}`);
  console.log(`🔌 WebSocket: ws://192.168.1.114:${PORT}/ws`);
  console.log(`💊 Health: http://192.168.1.114:${PORT}/health`);
  console.log(`⏰ Iniciado: ${new Date().toISOString()}`);
  console.log(`\n📡 Aguardando ESP8266...\n`);
});

// Tratamento de erros
server.on("error", (error) => {
  if (error.code === "EADDRINUSE") {
    console.log(`❌ Porta ${PORT} já está em uso!`);
    console.log(`💡 Mate o processo: taskkill /F /PID <PID>`);
  } else {
    console.error(`❌ Erro:`, error.message);
  }
});

// Parar graciosamente
process.on("SIGINT", () => {
  console.log(`\n🛑 Parando servidor...`);
  server.close(() => {
    console.log(`✅ Servidor parado!`);
    process.exit(0);
  });
});
