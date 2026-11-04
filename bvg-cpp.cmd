@echo off
setlocal
set "ROOT=%~dp0\bvg-cpp"
set "EXE=%ROOT%\build\Release\bvg-cpp-cli.exe"
if not exist "%EXE%" (
    echo bvg-cpp not built: %EXE%
    echo run: cd /d "%ROOT%" ^&^& cmake --build build --config Release --target bvg-cpp-cli
    exit /b 1
)
"%EXE%" %*