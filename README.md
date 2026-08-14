# 🎬 Media_TranscodeX

A high-performance, cross-platform standalone video and audio conversion and stream multiplexing utility built using **Modern C++17**, **FFmpeg C APIs**, and the **Qt 6 GUI framework**.

This project features low-level raw frame manipulation, advanced C++ Resource Acquisition Is Initialization (RAII) memory management patterns, sample-accurate audio buffering, and asynchronous multithreading for smooth media processing.

---

## ✨ Key Features

* ⚡ **Asynchronous Encoding Engine:** Transcoding and stream multiplexing run on a dedicated background `QThread`, keeping the GUI 100% responsive during intensive jobs.
* 📦 **Multi-Format Container Conversion:** Export videos to **MP4, MKV, WebM, AVI, QuickTime MOV, Flash FLV, MPEG-TS**, or extract audio to **MP3, WAV, FLAC**.
* 🎨 **Resolution Scaling & Color Conversion:** High-speed `libswscale` frame scaling with support for resolution presets (4K UHD, 2K QHD, 1080p, 720p, 480p, 360p) or custom Width/Height pixel dimensions.
* 🎵 **Multi-Track Stream Management:** Inspect container streams and interactively enable/disable internal tracks or multiplex external audio (`.mp3`, `.wav`, `.aac`, `.flac`, etc.) and subtitle files (`.srt`, `.vtt`, `.ass`).
* 🔊 **Fixed-Size Audio Buffering (`AVAudioFifo`):** Integrates `AVAudioFifo` and `libswresample` to buffer variable sample frames into fixed-size 1024-sample AAC blocks, preventing audio distortion or silent output.
* ⏱️ **Audio Timing & Duration Controls:** Precise audio start timestamp offsets (`av_seek_frame`) and automatic duration matching to align external audio length perfectly with the main video.
* 🛡️ **Modern C++ RAII Wrappers:** Native C-style FFmpeg allocations (`AVFrame`, `AVPacket`, `AVFormatContext`, `AVAudioFifo`, etc.) are managed via custom `std::unique_ptr` deleters to prevent memory leaks.
* 📊 **Real-Time Telemetry & Progress:** Signals and slots deliver frame-accurate PTS percentage updates directly to UI progress bars.
* ⏹️ **User Cancellation Control:** Cancel ongoing encoding jobs cleanly mid-stream with safe thread cleanup and file context flushing.

---

## 📊 Project Status & Progress Tracker

* **Current Status:** Sprints 1–7 Complete ✅ 🚀

| Sprint / Feature | Description / Goal | Status | Key Milestones |
| :--- | :--- | :---: | :--- |
| **Sprint 1** | **Environment & Build Setup** | ✅ Completed | • Integrated CMake build configuration with Qt 6 & FFmpeg C libraries.<br>• Verified C++17 build targets and project directory structure. |
| **Sprint 2** | **RAII Memory Management & Demuxing** | ✅ Completed | • Implemented C++ RAII smart pointer deleters for FFmpeg contexts.<br>• Created `MediaDemuxer` class to safely inspect containers and extract stream metadata. |
| **Sprint 3** | **Decoding Pipeline & Frame Access** | ✅ Completed | • Built `MediaDecoder` using `avcodec_send_packet()` and `avcodec_receive_frame()`.<br>• Implemented decoder flushing for end-of-stream leftover frames. |
| **Sprint 4** | **Frame Scaling & Color Conversion** | ✅ Completed | • Integrated `libswscale` inside custom `FrameScaler` engine.<br>• Added hardware YUV to RGB24 color space pixel conversion and frame scaling. |
| **Sprint 5** | **Encoding Pipeline & Muxing** | ✅ Completed | • Implemented `MediaEncoder` to allocate output container contexts and codecs.<br>• Handled timestamp rescaling (`av_packet_rescale_ts`) and container trailer flushing. |
| **Sprint 6** | **Multi-Track Stream Management** | ✅ Completed | • Built interactive tree manager to inspect/toggle internal video, audio, and subtitle streams.<br>• Added support for multiplexing external audio and subtitle files into output containers. |
| **Sprint 7** | **Audio FIFO Fix & Custom Formats/Scaling** | ✅ Completed | • Integrated `AVAudioFifo` buffering to resolve AAC variable sample size encoding bugs.<br>• Added external audio start offset and video duration auto-matching.<br>• Added container format selector (MP4, MKV, WEBM, AVI, MOV, FLV, TS, MP3, WAV) and custom resolution controls. |

---

## 🛠️ System Architecture

The application uses a decoupled multi-tier architecture:

```text
┌─────────────────────────────────────────────────────────────────────────┐
│                          LAYER 1: QT 6 FRONTEND                         │
│   (MainWindow, Stream Manager, Container & Resolution Options, GUI)    │
└────────────────────────────────────┬────────────────────────────────────┘
                                     │ Qt Signals / Slots
                                     ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                    LAYER 2: ASYNCHRONOUS WORKER                         │
│        (QThread Controller, Job Orchestrator, Multi-Pipeline Muxing)   │
└────────────────────────────────────┬────────────────────────────────────┘
                                     │ C++ Function Calls
                                     ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                     LAYER 3: FFMPEG ENGINE (C++)                        │
│   ┌──────────────────────┐  ┌──────────────────┐  ┌─────────────────┐   │
│   │ Smart Pointer RAII   │  │ MediaDemuxer /   │  │ MediaEncoder /  │   │
│   │ Wrappers (AVFrame)   │  │ MediaDecoder     │  │ AVAudioFifo     │   │
│   └──────────────────────┘  └──────────────────┘  └─────────────────┘   │
│   ┌──────────────────────┐  ┌──────────────────┐                        │
│   │ AudioResampler       │  │ FrameScaler      │                        │
│   │ (libswresample)      │  │ (libswscale)     │                        │
│   └──────────────────────┘  └──────────────────┘                        │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 📁 Repository Structure

```text
Media_TranscodeX/
├── CMakeLists.txt             # Cross-platform build configuration
├── README.md                  # Project documentation
├── src/
│   ├── core/                  # Pure C++ FFmpeg Engine
│   │   ├── RAIIWrappers.h     # Modern C++ smart pointer deleters (std::unique_ptr)
│   │   ├── TranscodeOptions.h # Unified transcode settings & container format defaults
│   │   ├── MediaDemuxer.h/.cpp# Demuxer engine & metadata extraction
│   │   ├── MediaDecoder.h/.cpp# Frame decoding pipeline
│   │   ├── MediaEncoder.h/.cpp# Frame encoding, container muxing & AVAudioFifo
│   │   ├── AudioResampler.h/.cpp # Audio sample rate/format conversion
│   │   └── FrameScaler.h/.cpp # Color space conversion & resolution frame scaling
│   ├── worker/                # Qt Threading & Orchestration
│   │   └── ConversionWorker.h/.cpp # Background worker task for multi-track processing
│   └── gui/                   # Qt User Interface
│       ├── main.cpp           # Application entry point
│       └── MainWindow.h/.cpp  # Desktop application dashboard & stream manager
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
git clone https://github.com/rakshi1701/MediaTranscodeX.git
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
