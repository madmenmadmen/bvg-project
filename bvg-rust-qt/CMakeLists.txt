cmake_minimum_required(VERSION 3.20)
project(bvg-rust-qt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Rust build via cargo ---
set(CARGO_TARGET_DIR "${CMAKE_BINARY_DIR}/cargo")

add_custom_command(
    OUTPUT "${CARGO_TARGET_DIR}/release/bvg_qt.lib"
    COMMAND ${CMAKE_COMMAND} -E env CARGO_TARGET_DIR=${CARGO_TARGET_DIR}
            cargo build --release
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/src/lib.rs
        ${CMAKE_CURRENT_SOURCE_DIR}/Cargo.toml
)

add_custom_target(bvg_qt_rust ALL DEPENDS "${CARGO_TARGET_DIR}/release/bvg_qt.lib")

# --- Qt6 ---
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

qt_standard_project_setup()

qt_add_executable(bvg-rust-qt-viewer
    cpp/main.cpp
    cpp/BvgCanvas.cpp
)

target_include_directories(bvg-rust-qt-viewer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_dependencies(bvg-rust-qt-viewer bvg_qt_rust)

# --- Link Rust library + Qt ---
target_link_libraries(bvg-rust-qt-viewer PRIVATE
    "${CARGO_TARGET_DIR}/release/bvg_qt.lib"
    Qt6::Core Qt6::Gui Qt6::Widgets
)

# --- Windows system libs (required by Rust std) ---
if(WIN32)
    target_link_libraries(bvg-rust-qt-viewer PRIVATE
        ws2_32
        ntdll
        userenv
        bcrypt
        advapi32
        kernel32
        user32
        shell32
        ole32
        oleaut32
        uuid
        gdi32
        comdlg32
        winspool
        wsock32
    )
endif()

# --- MSVC: ensure Rust staticlib symbols are linked ---
if(MSVC)
    target_link_options(bvg-rust-qt-viewer PRIVATE
        "/WHOLEARCHIVE:${CARGO_TARGET_DIR}/release/bvg_qt.lib")
endif()