#!/usr/bin/env node

/**
 * Gerador de CAR_ID único para placas ESP8266
 *
 * Uso:
 *   node generate-car-id.js
 *
 * Saída:
 *   -DCAR_ID_STR="CAR-1735689123456-a1b2c3d4e5"
 */

function generateCarId() {
  const timestamp = Date.now();
  // Gera string aleatória de 10 caracteres
  const random = Math.random().toString(36).substring(2, 12);
  return `CAR-${timestamp}-${random}`;
}

function main() {
  const carId = generateCarId();
  console.log(`\n🚗 Novo CAR_ID gerado:`);
  console.log(`-DCAR_ID_STR="${carId}"`);
  console.log(`\n📝 Cole esta linha no platformio.ini em build_flags`);
  console.log(`\n📋 Exemplo completo:`);
  console.log(`build_flags =`);
  console.log(`\t-DWS_HOST_STR="192.168.0.100"`);
  console.log(`\t-DCAR_ID_STR="${carId}"`);
  console.log();
}

if (require.main === module) {
  main();
}

module.exports = { generateCarId };
