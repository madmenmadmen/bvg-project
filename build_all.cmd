@echo off
setlocal enabledelayedexpansion

echo ============================================
echo  BVG build_all
echo ============================================
echo.

REM --- Java ---
echo [1/5] bvg-java (cli + viewer)
pushd "%~dp0bvg-java"
call gradlew.bat build --no-daemon
if errorlevel 1 (
    echo [WARN] bvg-java build failed, skipping
    popd
    goto :skip_java
)
popd
:skip_java
echo.

REM --- C++ ---
echo [2/5] bvg-cpp (lib + cli + viewer)
pushd "%~dp0bvg-cpp"
cmake -B build -G "Visual Studio 17 2022" -A x64
if errorlevel 1 (
    echo [WARN] bvg-cpp cmake configure failed, skipping
    popd
    goto :skip_cpp
)
cmake --build build --config Release --target bvg-cpp-cli
if errorlevel 1 (
    echo [WARN] bvg-cpp-cli build failed
)
cmake --build build --config Release --target bvg-viewer
if errorlevel 1 (
    echo "[WARN] bvg-viewer build failed (optional), continuing"
)
popd
:skip_cpp
echo.

REM --- Rust ---
echo [3/5] bvg-rust
pushd "%~dp0bvg-rust"
cargo build --release
if errorlevel 1 (
    echo [WARN] bvg-rust build failed, skipping
    popd
    goto :skip_rust
)
popd
:skip_rust
echo.

REM --- Rust + Qt ---
echo [4/5] bvg-rust-qt (optional viewer)
pushd "%~dp0bvg-rust-qt"
cmake -B build -G "Visual Studio 17 2022" -A x64
if errorlevel 1 (
    echo "[WARN] bvg-rust-qt cmake configure failed (optional), skipping"
    popd
    goto :skip_rust_qt
)
cmake --build build --config Release
if errorlevel 1 (
    echo "[WARN] bvg-rust-qt build failed (optional), continuing"
)
popd
:skip_rust_qt
echo.

REM --- WASM (web + nodejs) ---
echo [5/5] bvg-wasm (web + nodejs)
pushd "%~dp0bvg-wasm"
wasm-pack build --target web --out-dir www/pkg
if errorlevel 1 (
    echo "[WARN] bvg-wasm web build failed"
)
wasm-pack build --target nodejs --out-dir node/pkg
if errorlevel 1 (
    echo "[WARN] bvg-wasm nodejs build failed"
)
popd
echo.

echo ============================================
echo  all done (with warnings, see above)
echo ============================================
exit /b 0