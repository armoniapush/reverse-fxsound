<#
.SYNOPSIS
    Instala los plugins de x64dbg MCP (.dp64 / .dp32) en la instalacion de x64dbg.
.DESCRIPTION
    Copia los binarios precompilados de C:\Users\armon\tools\x64dbg_mcp\release-bin
    hacia las carpetas x64\plugins y x32\plugins de x64dbg.
#>

[CmdletBinding()]
param(
    [string]$Path
)

$ErrorActionPreference = 'Stop'
$BinSource = "C:\Users\armon\tools\x64dbg_mcp\release-bin"

function Test-X64dbgRoot([string]$dir) {
    if ([string]::IsNullOrWhiteSpace($dir) -or -not (Test-Path $dir)) { return $false }
    return (Test-Path (Join-Path $dir 'x64\x64dbg.exe')) -or (Test-Path (Join-Path $dir 'x32\x32dbg.exe'))
}

function Find-X64dbg {
    # 0. Ruta comun directa
    if (Test-X64dbgRoot "C:\x64dbg") { return "C:\x64dbg" }

    # 1. Proceso activo
    foreach ($name in 'x96dbg', 'x64dbg', 'x32dbg') {
        $p = Get-Process -Name $name -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($p -and $p.Path) {
            $dir = Split-Path -Parent $p.Path
            if (Test-X64dbgRoot $dir) { return $dir }
            $parent = Split-Path -Parent $dir
            if (Test-X64dbgRoot $parent) { return $parent }
        }
    }

    # 2. PATH
    $onPath = (Get-Command x96dbg.exe -ErrorAction SilentlyContinue).Source
    if ($onPath) {
        $dir = Split-Path -Parent $onPath
        if (Test-X64dbgRoot $dir) { return $dir }
    }

    # 3. Registry
    try {
        $cmd = (Get-ItemProperty 'Registry::HKEY_CLASSES_ROOT\exefile\shell\Debug with x64dbg\Command' -ErrorAction Stop).'(default)'
        if ($cmd -match '"([^"]+x96dbg\.exe)"') {
            $dir = Split-Path -Parent $Matches[1]
            if (Test-X64dbgRoot $dir) { return $dir }
        }
    } catch {}

    return $null
}

Write-Host "=== Instalador de Plugin MCP para x64dbg ===" -ForegroundColor Cyan

$targetDir = $Path
if (-not $targetDir) {
    $targetDir = Find-X64dbg
}

if (-not $targetDir -or -not (Test-X64dbgRoot $targetDir)) {
    Write-Host "[!] No se detecto la ruta de x64dbg automaticamente." -ForegroundColor Yellow
    Write-Host "    Introduce la carpeta raiz de x64dbg (donde se encuentran las subcarpetas x64 y x32):"
    $targetDir = Read-Host "    Ruta"
}

if (-not (Test-X64dbgRoot $targetDir)) {
    Write-Error "La ruta especificada no es valida (falta x64\x64dbg.exe o x32\x32dbg.exe)."
    exit 1
}

Write-Host "[+] Directorio detectado: $targetDir" -ForegroundColor Green

# x64
$p64 = Join-Path $targetDir "x64\plugins"
if (Test-Path $p64) {
    Copy-Item -Path (Join-Path $BinSource "x64dbg_mcp.dp64") -Destination $p64 -Force
    Write-Host "[✓] x64dbg_mcp.dp64 copiado a $p64" -ForegroundColor Green
} else {
    Write-Host "[!] Carpeta $p64 no encontrada." -ForegroundColor Yellow
}

# x32
$p32 = Join-Path $targetDir "x32\plugins"
if (Test-Path $p32) {
    Copy-Item -Path (Join-Path $BinSource "x64dbg_mcp.dp32") -Destination $p32 -Force
    Write-Host "[✓] x64dbg_mcp.dp32 copiado a $p32" -ForegroundColor Green
} else {
    Write-Host "[!] Carpeta $p32 no encontrada." -ForegroundColor Yellow
}

Write-Host "`n[✓] Instalacion completada. Inicia o recarga x64dbg para activar el servidor en 127.0.0.1:27042." -ForegroundColor Cyan
