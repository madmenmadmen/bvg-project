@echo off
setlocal
set "ROOT=%~dp0\bvg-rust"
set "EXE=%ROOT%\target\release\bvg-rust.exe"
if not exist "%EXE%" (
    echo bvg-rust not built: %EXE%
    echo run: cd /d "%ROOT%" ^&^& cargo build --release --bin bvg-rust
    exit /b 1
)
"%EXE%" %*