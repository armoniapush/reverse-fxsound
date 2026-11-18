# Reglas de Agente para Reverse Engineering (IDA Pro 9.3 & x64dbg)

Este proyecto está configurado para interactuar directamente con **IDA Pro 9.3** y **x64dbg** a través de servidores MCP (Model Context Protocol).

## 1. Arquitectura de Conectividad MCP

### IDA Professional 9.3 (`ida-pro`)
* **Transporte**: stdio / RPC interno.
* **Servidor MCP**: `ida-pro-mcp.exe` (vía `~/.local/bin/ida-pro-mcp.exe`).
* **Plugin IDA**: `ida_mcp.py` ubicado en `%APPDATA%\Hex-Rays\IDA Pro\plugins`.
* **Puerto RPC**: `127.0.0.1:13337`.
* **Herramientas disponibles**: Decompilación de funciones, listado de funciones y segmentos, lectura y modificación de tipos (`set_type`), comentarios (`set_comment`), referencias cruzadas (`get_xrefs`), renombrado de variables y funciones (`rename`).

### x64dbg / x32dbg (`x64dbg`)
* **Transporte**: stdio / REST local.
* **Servidor MCP**: `node C:\Users\armon\tools\x64dbg_mcp\server\dist\index.js`.
* **Plugin x64dbg**: `x64dbg_mcp.dp64` en `x64/plugins/` (y `x64dbg_mcp.dp32` en `x32/plugins/`).
* **Puerto interno**: `127.0.0.1:27042`.
* **Herramientas disponibles**: Control de breakpoints (`set_breakpoint`, `delete_breakpoint`), ejecución (`run`, `step_into`, `step_over`), lectura/escritura de memoria y registros, desensamblado dinámico, trace logging, bypass de anti-debug y dump de módulos PE.

---

## 2. Flujo de Trabajo Recomendado

1. **Análisis Estático (IDA Pro 9.3)**:
   - Abrir el binario objetivo en IDA Pro 9.3.
   - El plugin `ida_mcp` se activa automáticamente levantando el servicio en `127.0.0.1:13337`.
   - Utilizar las herramientas MCP de IDA para identificar funciones clave, tablas de saltos y estructuras de datos.

2. **Análisis Dinámico (x64dbg)**:
   - Cargar el binario en x64dbg.
   - El plugin `x64dbg_mcp.dp64` iniciará el listener en `127.0.0.1:27042`.
   - Colocar breakpoints en las direcciones o APIs descubiertas durante el análisis estático.
   - Inspeccionar registros y memoria en tiempo real durante la ejecución.

3. **Verificación de Conectividad**:
   - Ejecutar `powershell -ExecutionPolicy Bypass -File .\scripts\check-status.ps1` para verificar si ambos puertos locales están abiertos y respondiendo.
