cmake_minimum_required(VERSION 3.20)
project(bvg LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Vendored dependencies ---
set(THIRD_PARTY "${CMAKE_CURRENT_SOURCE_DIR}/../third_party")

# zlib (shared)
add_library(ZLIB::ZLIB SHARED IMPORTED)
set_target_properties(ZLIB::ZLIB PROPERTIES
    IMPORTED_IMPLIB "${THIRD_PARTY}/lib/z.lib"
    IMPORTED_LOCATION "${THIRD_PARTY}/bin/z.dll"
    INTERFACE_INCLUDE_DIRECTORIES "${THIRD_PARTY}/include"
)

# zstd (shared)
add_library(zstd::libzstd_shared SHARED IMPORTED)
set_target_properties(zstd::libzstd_shared PROPERTIES
    IMPORTED_IMPLIB "${THIRD_PARTY}/lib/zstd.lib"
    IMPORTED_LOCATION "${THIRD_PARTY}/bin/zstd.dll"
    INTERFACE_INCLUDE_DIRECTORIES "${THIRD_PARTY}/include"
)

# lz4 (shared)
add_library(lz4::lz4 SHARED IMPORTED)
set_target_properties(lz4::lz4 PROPERTIES
    IMPORTED_IMPLIB "${THIRD_PARTY}/lib/lz4.lib"
    IMPORTED_LOCATION "${THIRD_PARTY}/bin/lz4.dll"
    INTERFACE_INCLUDE_DIRECTORIES "${THIRD_PARTY}/include"
)

# --- bvg library ---
add_library(bvg STATIC
    src/Codec.cpp
    src/BvgFile.cpp
    src/Document.cpp
    src/Cli.cpp
)
target_include_directories(bvg PUBLIC include)
target_link_libraries(bvg PUBLIC
    ZLIB::ZLIB
    zstd::libzstd_shared
    lz4::lz4
)

# --- CLI ---
add_executable(bvg-cpp-cli
    src/main.cpp
)
target_link_libraries(bvg-cpp-cli PRIVATE bvg)

# --- GUI (Qt6) ---
find_package(Qt6 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()
qt_add_executable(bvg-viewer
    viewer/BvgViewer.cpp
    viewer/BvgCanvas.cpp
)
target_link_libraries(bvg-viewer PRIVATE bvg Qt6::Widgets)

# --- Copy DLLs next to executables (Windows) ---
if(WIN32)
    add_custom_command(TARGET bvg-cpp-cli POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${THIRD_PARTY}/bin/z.dll"
            "${THIRD_PARTY}/bin/zstd.dll"
            "${THIRD_PARTY}/bin/lz4.dll"
            $<TARGET_FILE_DIR:bvg-cpp-cli>
    )
    add_custom_command(TARGET bvg-viewer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${THIRD_PARTY}/bin/z.dll"
            "${THIRD_PARTY}/bin/zstd.dll"
            "${THIRD_PARTY}/bin/lz4.dll"
            $<TARGET_FILE_DIR:bvg-viewer>
    )
endif()