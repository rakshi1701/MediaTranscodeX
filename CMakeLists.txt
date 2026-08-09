cmake_minimum_required(VERSION 3.16)
project(Media_TranscodeX LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Concurrent)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
    libavcodec
    libavformat
    libavutil
    libswscale
    libswresample
)

# --- Define Executable Sources ---
add_executable(Media_TranscodeX
    src/gui/main.cpp
    src/gui/MainWindow.cpp
    src/worker/ConversionWorker.cpp
    src/core/MediaDemuxer.cpp
    src/core/MediaDecoder.cpp
    src/core/FrameScaler.cpp
    src/core/MediaEncoder.cpp
)

# --- Target Include Directories ---
target_include_directories(Media_TranscodeX PRIVATE
    src/core
    src/gui
    src/worker
    
)

target_link_libraries(Media_TranscodeX PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Concurrent
    PkgConfig::FFMPEG
)