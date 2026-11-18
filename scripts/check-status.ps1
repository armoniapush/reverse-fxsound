# Diagnostico de estado de los entornos MCP (IDA Pro 9.3 y x64dbg)

function Test-PortOpen([string]$hostName, [int]$port, [int]$timeoutMs = 300) {
    try {
        $client = [System.Net.Sockets.TcpClient]::new()
        $iar = $client.BeginConnect($hostName, $port, $null, $null)
        $success = $iar.AsyncWaitHandle.WaitOne($timeoutMs)
        if ($success) {
            $client.EndConnect($iar)
            $client.Close()
            return $true
        }
        $client.Close()
        return $false
    } catch {
        return $false
    }
}

Write-Host "=============================================" -ForegroundColor Cyan
Write-Host " Diagnostico de MCP: IDA Pro 9.3 & x64dbg   " -ForegroundColor Cyan
Write-Host "=============================================" -ForegroundColor Cyan

# 1. IDA Pro RPC Port
$idaPort = 13337
if (Test-PortOpen "127.0.0.1" $idaPort) {
    Write-Host "[✓] IDA Pro RPC (127.0.0.1:$idaPort): ACTIVO (Conectado)" -ForegroundColor Green
} else {
    Write-Host "[!] IDA Pro RPC (127.0.0.1:$idaPort): INACTIVO (Abre IDA Pro 9.3 con el plugin ida_mcp)" -ForegroundColor Yellow
}

# 2. x64dbg MCP Port
$x64Port = 27042
if (Test-PortOpen "127.0.0.1" $x64Port) {
    Write-Host "[✓] x64dbg Plugin (127.0.0.1:$x64Port): ACTIVO (Conectado)" -ForegroundColor Green
} else {
    Write-Host "[!] x64dbg Plugin (127.0.0.1:$x64Port): INACTIVO (Inicia x64dbg con x64dbg_mcp cargado)" -ForegroundColor Yellow
}

# 3. Binarios y Ejecutables
Write-Host "`n--- Verificacion de Ejecutables ---" -ForegroundColor Cyan
$idaExe = (Get-Command ida-pro-mcp.exe -ErrorAction SilentlyContinue).Source
if ($idaExe) {
    Write-Host "[✓] ida-pro-mcp.exe: $idaExe" -ForegroundColor Green
} else {
    Write-Host "[✗] ida-pro-mcp.exe no encontrado en PATH" -ForegroundColor Red
}

$x64Server = "C:\Users\armon\tools\x64dbg_mcp\server\dist\index.js"
if (Test-Path $x64Server) {
    Write-Host "[✓] x64dbg-mcp-server: $x64Server" -ForegroundColor Green
} else {
    Write-Host "[✗] x64dbg-mcp-server dist/index.js no encontrado" -ForegroundColor Red
}

$dp64Installed = "C:\x64dbg\x64\plugins\x64dbg_mcp.dp64"
if (Test-Path $dp64Installed) {
    Write-Host "[✓] Plugin instalado en x64dbg: $dp64Installed" -ForegroundColor Green
} else {
    Write-Host "[!] Plugin no instalado en C:\x64dbg\x64\plugins" -ForegroundColor Yellow
}

$reaExe = (Get-Command rea.cmd -ErrorAction SilentlyContinue).Source
if ($reaExe) {
    Write-Host "[✓] REA CLI (rea-agents): $reaExe" -ForegroundColor Green
} else {
    Write-Host "[✗] rea.cmd no encontrado" -ForegroundColor Red
}

Write-Host "=============================================" -ForegroundColor Cyan
