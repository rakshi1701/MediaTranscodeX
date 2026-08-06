cmake_minimum_required(VERSION 3.16)
project(Media_TranscodeX LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Automatically enable Qt MOC (Meta-Object Compiler), UIC, and RCC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# --- 1. Find Qt 6 ---
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Concurrent)

# --- 2. Find FFmpeg ---
# (Using pkg-config is standard for Linux/macOS. Windows may require manual paths)
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
    libavcodec
    libavformat
    libavutil
    libswscale
    libswresample
)

# --- 3. Define the Executable ---
add_executable(Media_TranscodeX
    src/gui/main.cpp
)

# --- 4. Link Libraries ---
target_link_libraries(Media_TranscodeX PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Concurrent
    PkgConfig::FFMPEG
)