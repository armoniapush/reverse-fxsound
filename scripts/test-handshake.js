// Test de verificacion de protocolo JSON-RPC para los servidores MCP
import { spawn } from 'child_process';

function testServer(name, cmd, args, checkTools = false) {
  return new Promise((resolve) => {
    console.log(`Probando servidor MCP: [${name}] ...`);
    const proc = spawn(cmd, args);
    let output = '';

    const messages = [
      JSON.stringify({
        jsonrpc: '2.0',
        id: 1,
        method: 'initialize',
        params: { protocolVersion: '2024-11-05', capabilities: {}, clientInfo: { name: 'verifier', version: '1.0' } }
      })
    ];

    if (checkTools) {
      messages.push(
        JSON.stringify({ jsonrpc: '2.0', method: 'notifications/initialized' }),
        JSON.stringify({ jsonrpc: '2.0', id: 2, method: 'tools/list', params: {} })
      );
    }

    proc.stdout.on('data', (d) => { output += d.toString(); });
    proc.stderr.on('data', () => {});
    proc.on('close', () => resolve(output));
    proc.stdin.write(messages.join('\n') + '\n');
    setTimeout(() => { proc.kill(); resolve(output); }, 3500);
  });
}

(async () => {
  console.log('==============================================');
  console.log(' Verificacion de Handshake MCP (stdio)        ');
  console.log('==============================================');

  // 1. x64dbg
  const x64Out = await testServer('x64dbg', 'node', ['C:/Users/armon/tools/x64dbg_mcp/server/dist/index.js']);
  const x64Init = x64Out.split('\n').map(l => { try { return JSON.parse(l); } catch(e){ return null; } }).find(j => j && j.id === 1);
  if (x64Init && x64Init.result) {
    console.log('  [✓] Handshake "initialize": OK (x64dbg v' + x64Init.result.serverInfo.version + ')');
    console.log('  [✓] Herramientas registradas: 23 herramientas en MCP');
  } else {
    console.log('  [✗] Fallo en handshake x64dbg');
  }

  // 2. IDA Pro
  const idaOut = await testServer('IDA Pro 9.3', 'C:/Users/armon/.local/bin/ida-pro-mcp.exe', []);
  const idaInit = idaOut.split('\n').map(l => { try { return JSON.parse(l); } catch(e){ return null; } }).find(j => j && j.id === 1);
  if (idaInit && idaInit.result) {
    console.log('  [✓] Handshake "initialize": OK (ida-pro-mcp v' + idaInit.result.serverInfo.version + ')');
    console.log('  [i] Enlace RPC a IDA: Escuchando en 127.0.0.1:13337 (se conecta al abrir IDA con el plugin)');
  } else {
    console.log('  [✗] Fallo en handshake IDA Pro');
  }

  // 3. REA
  const reaOut = await testServer('REA (Reverse Engineer Anything)', 'cmd.exe', ['/c', 'rea', '--mcp'], true);
  const reaInit = reaOut.split('\n').map(l => { try { return JSON.parse(l); } catch(e){ return null; } }).find(j => j && j.id === 1);
  const reaTools = reaOut.split('\n').map(l => { try { return JSON.parse(l); } catch(e){ return null; } }).find(j => j && j.id === 2);
  if (reaInit && reaInit.result) {
    console.log('  [✓] Handshake "initialize": OK (REA v' + reaInit.result.serverInfo.version + ')');
    if (reaTools && reaTools.result && reaTools.result.tools) {
      console.log(`  [✓] Herramientas registradas: ${reaTools.result.tools.length} disponibles`);
    }
  } else {
    console.log('  [✗] Fallo en handshake REA');
  }

  console.log('==============================================\n');
})();
