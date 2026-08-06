Here is your complete, production-ready **`README.md`** file for the project repository. You can copy and paste this directly into a `README.md` file in the root directory of your project.

---

# 🎬 Modern C++ & Qt Media Converter Application

A high-performance, cross-platform standalone video and audio conversion utility built using **Modern C++17**, **FFmpeg C APIs**, and **Qt 6 GUI framework**.

This project implements low-level raw frame manipulation, advanced C++ Resource Acquisition Is Initialization (RAII) memory management patterns, and asynchronous multithreading for smooth media processing.

---

## 🛠️ Architecture Overview

The system is decoupled into three distinct architectural layers to ensure stability, maintainability, and responsiveness:

```text
┌─────────────────────────────────────────────────────────────────────────┐
│                          LAYER 1: QT 6 FRONTEND                         │
│   (MainWindow, File Selector, Codec Options, Progress Bar, OpenGL/QImage) │
└────────────────────────────────────┬────────────────────────────────────┘
                                     │ Qt Signals / Slots
                                     ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                    LAYER 2: ASYNCHRONOUS WORKER                         │
│        (QThread Controller, Job Queue, Thread-Safe Frame Passing)       │
└────────────────────────────────────┬────────────────────────────────────┘
                                     │ C++ Function Calls
                                     ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                     LAYER 3: FFMPEG ENGINE (C++)                        │
│   ┌──────────────────────┐  ┌──────────────────┐  ┌─────────────────┐   │
│   │ Smart Pointer RAII   │  │ Decode Loop      │  │ Encode Loop     │   │
│   │ Wrappers (AVFrame)   │  │ (Demux/Decode)   │  │ (Filter/Encode) │   │
│   └──────────────────────┘  └──────────────────┘  └─────────────────┘   │
└─────────────────────────────────────────────────────────────────────────┘

```

### Core Design Rules

1. **Asynchronous Execution:** No heavy decoding or encoding runs on the main Qt GUI thread. All processing occurs inside a dedicated `QThread` worker.
2. **Zero Naked Pointers:** All FFmpeg C structs (`AVFrame`, `AVPacket`, `AVFormatContext`, etc.) are wrapped using custom C++ smart pointers (`std::unique_ptr` with custom deleters) to eliminate memory leaks.
3. **Decoupled Engine:** The low-level C++ FFmpeg backend has no direct dependency on Qt, allowing it to be compiled or tested independently.

---

## 📁 Repository Structure

```text
FFmpegQtConverter/
├── CMakeLists.txt             # Cross-platform build configuration
├── README.md                  # Project documentation
├── src/
│   ├── core/                  # Pure C++ FFmpeg Engine (No Qt dependencies)
│   │   ├── RAIIWrappers.h     # Smart pointer deleters (std::unique_ptr)
│   │   ├── MediaDemuxer.h/.cpp
│   │   ├── MediaDecoder.h/.cpp
│   │   ├── MediaEncoder.h/.cpp
│   │   └── FrameScaler.h/.cpp
│   ├── worker/                # Qt Threading & Orchestration
│   │   ├── ConversionWorker.h/.cpp
│   │   └── JobQueue.h/.cpp
│   └── gui/                   # Qt User Interface
│       ├── MainWindow.h/.cpp
│       ├── MainWindow.ui
│       └── VideoPreviewWidget.h/.cpp
└── tests/                     # Unit tests for core C++ engine

```

---

## 🗺️ Master Agile Roadmap (6 Sprints)

The project development is structured into six 2-week sprints:

```text
  Sprint 1         Sprint 2         Sprint 3         Sprint 4         Sprint 5         Sprint 6
┌──────────┐     ┌──────────┐     ┌──────────┐     ┌──────────┐     ┌──────────┐     ┌──────────┐
│ Env Setup│ ──> │ RAII &   │ ──> │ Decoding │ ──> │ Filtering│ ──> │ Encoding │ ──> │ Qt GUI & │
│ & CMake  │     │ Demuxer  │     │ Pipeline │     │ & Rescale│     │ & Muxing │     │ Threads  │
└──────────┘     └──────────┘     └──────────┘     └──────────┘     └──────────┘     └──────────┘

```

| Sprint | Objective | Key Deliverables & Concepts |
| --- | --- | --- |
| **Sprint 1** | **Environment & Build Setup** | • Configure CMake for Qt 6 and FFmpeg (`avcodec`, `avformat`, `avutil`, `swscale`, `swresample`).<br> • Verify C++17<br>• Set up initial folder structure. |
| **Sprint 2** | **RAII Memory Management & Demuxing** | • Build custom C++ deleters for FFmpeg pointers.<br>• Implement `MediaDemuxer` class to extract container metadata.<br>• Perform Valgrind / AddressSanitizer checks for zero memory leaks. |
| **Sprint 3** | **Decoding Pipeline & Frame Access** | • Build decode loop (`av_read_frame`, `avcodec_send_packet`, `avcodec_receive_frame`).<br>• Manage reference counting (`av_frame_ref` / `av_frame_unref`).<br>• Extract raw uncompressed audio/video `AVFrame` instances. |
| **Sprint 4** | **Frame Scaling & Color Conversion** | • Integrate `libswscale` for color space conversion (YUV420p $\rightarrow$ RGB24).<br>• Wrap scaling contexts in RAII smart pointers.<br>• Convert video frames to `QImage` for live frontend previewing. |
| **Sprint 5** | **Encoding Pipeline & Container Muxing** | • Initialize encoder contexts and target container formats.<br>• Handle timestamp rescaling (`av_rescale_q`). <br>• Write interleaved media packets to disk and handle codec flushing routines. |
| **Sprint 6** | **Qt UI Integration & Threading** | • Connect engine to GUI using `QThread` and Signals/Slots. <br>• Build user interface controls (progress bars, preset dropdowns, format selection).<br>• End-to-end stress testing and leak checks. |

---

## 📋 Prerequisites & Dependencies

To build and run this project, ensure you have the following installed on your host system:

* **Compiler:** C++17 compliant compiler (GCC 9+, Clang 10+, or MSVC 2019+)
* **Build System:** CMake (v3.16 or higher)
* **GUI Framework:** Qt 6 (Core, Gui, Widgets, Concurrent)
* **Media Libraries:** FFmpeg development libraries (`libavcodec`, `libavformat`, `libavutil`, `libswscale`, `libswresample`)

---

## 💻 Build Instructions

### 1. Clone the Repository

```bash
git clone https://github.com/your-username/FFmpegQtConverter.git
cd FFmpegQtConverter

```

### 2. Configure and Build via CMake

```bash
# Generate build files
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release

# Compile project
cmake --build build --config Release

```

### 3. Run the Application

```bash
# Linux/macOS
./build/FFmpegQtConverter

# Windows
.\build\Release\FFmpegQtConverter.exe

```

---

## 📄 License

This project is licensed under the **MIT License** - see the `LICENSE` file for details. Note that FFmpeg binaries/libraries may fall under GPL/LGPL depending on how they are compiled.