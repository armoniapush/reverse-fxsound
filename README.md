# Reverse Engineering Workspace: FxSound (IDA Pro 9.3 & x64dbg MCP)

Espacio de trabajo configurado para ingeniería inversa y análisis binario integrado con **IDA Professional 9.3** y **x64dbg** mediante el protocolo **MCP (Model Context Protocol)**.

---

## 🛠️ Componentes y Configuración

| Herramienta | Servidor MCP | Endpoint Local | Ubicación del Plugin |
| :--- | :--- | :--- | :--- |
| **IDA Pro 9.3** | `ida-pro-mcp.exe` | `127.0.0.1:13337` | `%APPDATA%\Hex-Rays\IDA Pro\plugins\ida_mcp.py` |
| **x64dbg** | `x64dbg-mcp-server` (`node`) | `127.0.0.1:27042` | `C:\x64dbg\x64\plugins\x64dbg_mcp.dp64` |
| **REA** | `rea --mcp` | Local stdio | Instalado global (`rea-agents` v6.7.0) |

---

## 🚀 Inicio Rápido

### 1. Verificación de Estado
Para comprobar si los plugins de IDA Pro y x64dbg están activos y escuchando en sus puertos locales:
```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\check-status.ps1
```

### 2. Instalación del Plugin en x64dbg
Si aún no has copiado los archivos de plugin en tu carpeta de x64dbg:
```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\install-x64dbg-plugin.ps1
```
*(Detectará automáticamente tu instalación o te solicitará la ruta del directorio release de x64dbg).*

### 3. IDA Pro 9.3
El plugin `ida_mcp.py` ya se encuentra instalado y enlazado en `%APPDATA%\Hex-Rays\IDA Pro\plugins`. 
* Al abrir cualquier archivo en IDA Pro 9.3, el plugin inicia automáticamente el servidor RPC en el puerto `13337`.

---

## ⚙️ Archivos de Configuración MCP Generados

* **Antigravity Global**: `~/.gemini/config/mcp_config.json` (actualizado automáticamente).
* **Universal MCP / Claude**: [`.mcp.json`](file:///C:/Users/armon/DEV/reverse-fxsound/.mcp.json)
* **Cursor**: [`.cursor/mcp.json`](file:///C:/Users/armon/DEV/reverse-fxsound/.cursor/mcp.json)
* **VS Code**: [`.vscode/mcp.json`](file:///C:/Users/armon/DEV/reverse-fxsound/.vscode/mcp.json)

---

## 🔍 Capacidades Disponibles para el Agente

### Vía IDA Pro 9.3 (`ida-pro`)
* Decompilación automática de funciones a pseudocódigo C.
* Inspección de tipos de datos, estructuras y llamadas cruzadas (xrefs).
* Renombrado de símbolos y adición de comentarios analíticos en el IDB.

### Vía x64dbg (`x64dbg`)
* Colocación y gestión de breakpoints dinámicos por dirección o función API.
* Control de flujo de depuración: `step_into`, `step_over`, `run`, `pause`.
* Lectura y modificación de registros CPU y bloques de memoria en tiempo de ejecución.
* Detección y evasión de mecanismos anti-debugging.
