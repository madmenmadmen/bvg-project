@echo off
setlocal

echo ============================================
echo  BVG build_all
echo ============================================
echo.

REM --- Java ---
echo [1/5] bvg-java (cli + viewer)
pushd "%~dp0bvg-java"
call gradlew.bat build --no-daemon
if errorlevel 1 goto :err
popd
echo.

REM --- C++ ---
echo [2/5] bvg-cpp (lib + cli + viewer)
pushd "%~dp0bvg-cpp"
cmake -B build -G "Visual Studio 17 2022" -A x64
if errorlevel 1 goto :err
cmake --build build --config Release --target bvg-cpp-cli
if errorlevel 1 goto :err
cmake --build build --config Release --target bvg-viewer
if errorlevel 1 goto :err
popd
echo.

REM --- Rust ---
echo [3/5] bvg-rust
pushd "%~dp0bvg-rust"
cargo build --release
if errorlevel 1 goto :err
popd
echo.

REM --- Rust + Qt ---
echo [4/5] bvg-rust-qt
pushd "%~dp0bvg-rust-qt"
cmake -B build -G "Visual Studio 17 2022" -A x64
if errorlevel 1 goto :err
cmake --build build --config Release
if errorlevel 1 goto :err
popd
echo.

REM --- WASM (web + nodejs) ---
echo [5/5] bvg-wasm (web + nodejs)
pushd "%~dp0bvg-wasm"
wasm-pack build --target web --out-dir www/pkg
if errorlevel 1 goto :err
wasm-pack build --target nodejs --out-dir node/pkg
if errorlevel 1 goto :err
popd
echo.

echo ============================================
echo  all done
echo ============================================
exit /b 0

:err
echo.
echo ============================================
echo  BUILD FAILED
echo ============================================
popd
exit /b 1