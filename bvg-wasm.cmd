@echo off
setlocal
set "ROOT=%~dp0\bvg-wasm"
if not exist "%ROOT%\node\pkg\bvg_wasm.js" (
    echo bvg-wasm not built: %ROOT%\www\pkg\bvg_wasm.js
    exit /b 1
)
node "%ROOT%\node\wasm_dump.js" %*