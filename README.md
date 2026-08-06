
# 🎬 Media_TranscodeX

A high-performance, cross-platform standalone video and audio conversion utility built using **Modern C++17**, **FFmpeg C APIs**, and the **Qt 6 GUI framework**.

This project features low-level raw frame manipulation, advanced C++ Resource Acquisition Is Initialization (RAII) memory management patterns, and asynchronous multithreading for smooth media processing.

---

## 📊 Project Status & Progress Tracker

* **Current Sprint:** Sprint 2 Complete ✅ | **Next Up:** Sprint 3 (Decoding Pipeline & Frame Access) 🚀

| Sprint | Goal / Feature | Status | Key Milestones |
| --- | --- | --- | --- |
| **Sprint 1** | **Environment & Build Setup** | ✅ Completed | • Integrated CMake build configuration with Qt 6 & FFmpeg C libraries.<br>

<br>• Verified C++17 build targets and project directory structure.<br>

<br>• Successfully launched initial test window with FFmpeg runtime linking. |
| **Sprint 2** | **RAII Memory Management & Demuxing** | ✅ Completed | • Implemented custom C++ RAII smart pointer deleters (`std::unique_ptr`) for `AVFormatContext`, `AVCodecContext`, `AVPacket`, and `AVFrame`.<br>

<br>• Created `MediaDemuxer` class to safely open containers and extract stream metadata.<br>

<br>• Added dynamic file selection using native Qt `QFileDialog`. |
| **Sprint 3** | **Decoding Pipeline & Frame Access** | ⏳ Pending | • Build low-level frame decoding loop (`av_read_frame`, `avcodec_send_packet`, `avcodec_receive_frame`).<br>

<br>• Manage buffer reference counting (`av_frame_ref` / `av_frame_unref`).<br>

<br>• Extract raw uncompressed audio and video `AVFrame` buffers. |
| **Sprint 4** | **Frame Scaling & Color Conversion** | ⏳ Pending | • Integrate `libswscale` for YUV420p to RGB24 conversion.<br>

<br>• Convert video frames to `QImage` for live frontend GUI previewing. |
| **Sprint 5** | **Encoding Pipeline & Container Muxing** | ⏳ Pending | • Initialize encoder contexts and target container formats.<br>

<br>• Rescale media timestamps (`av_rescale_q`) and write interleaved streams to disk. |
| **Sprint 6** | **Qt UI Integration & Threading** | ⏳ Pending | • Connect core C++ engine to Qt UI via `QThread` and Signals/Slots.<br>

<br>• Build conversion queue, progress bars, codec dropdowns, and preset settings. |

---

## 🛠️ System Architecture

The application uses a three-tier decoupled architecture:

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

---

## 📁 Repository Structure

```text
Media_TranscodeX/
├── CMakeLists.txt             # Cross-platform build configuration
├── README.md                  # Project documentation & sprint updates
├── src/
│   ├── core/                  # Pure C++ FFmpeg Engine (No Qt dependencies)
│   │   ├── RAIIWrappers.h     # Modern C++ smart pointer deleters (std::unique_ptr)
│   │   ├── MediaDemuxer.h/.cpp# Demuxer engine & metadata extraction
│   │   ├── MediaDecoder.h/.cpp# (Sprint 3) Frame decoding pipeline
│   │   ├── MediaEncoder.h/.cpp# (Sprint 5) Frame encoding pipeline
│   │   └── FrameScaler.h/.cpp # (Sprint 4) Color space & image scaling
│   ├── worker/                # Qt Threading & Orchestration (Sprint 6)
│   │   ├── ConversionWorker.h/.cpp
│   │   └── JobQueue.h/.cpp
│   └── gui/                   # Qt User Interface
│       ├── main.cpp           # Entry point and Sprint test harness
│       ├── MainWindow.h/.cpp  # (Sprint 6) Main desktop user interface
│       └── VideoPreviewWidget.h/.cpp
└── tests/                     # Unit tests for core engine

```

---

## 📋 Prerequisites & Dependencies

To compile and run **Media_TranscodeX**, ensure your host machine has:

* **OS:** Linux (Ubuntu 20.04+ / Debian 11+ recommended)
* **Compiler:** C++17 compliant compiler (`g++` or `clang++`)
* **Build System:** CMake (v3.16+)
* **Libraries:**
* Qt 6 (`qt6-base-dev`)
* FFmpeg Development Libraries (`libavcodec-dev`, `libavformat-dev`, `libavutil-dev`, `libswscale-dev`, `libswresample-dev`)



---

## 💻 Build & Execution Instructions

### 1. Build the Application

```bash
# Clone the repository
git clone https://github.com/your-username/Media_TranscodeX.git
cd Media_TranscodeX

# Create build directory and run CMake
mkdir -p build && cd build
cmake ..

# Compile using all CPU cores
make -j$(nproc)

```

### 2. Run the Application

```bash
./Media_TranscodeX

```

---

## 📄 License

This project is licensed under the **MIT License**.

---

### Ready for Sprint 3?

Whenever you are ready, let me know, and we will write the **`MediaDecoder`** class to parse compressed packets into raw `AVFrame` instances!