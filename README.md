# 🎬 Media_TranscodeX

A high-performance video/audio transcoder, format converter, and multi-track stream multiplexing tool, available as two independent implementations:

- **Desktop app** (this document) — standalone **Modern C++17**, **FFmpeg C APIs**, and **Qt 6** application.
- **Web app** (`web/`) — runs entirely client-side in the browser via **ffmpeg.wasm**, no install or server-side processing required. See the "🌐 Web Version (Browser-Based)" section below.

Media_TranscodeX provides low-level raw frame decoding, hardware-accelerated color/resolution scaling, sample-accurate audio frame buffering (`AVAudioFifo`), interactive track stream management, and non-blocking asynchronous multithreading.

---

## 🏗️ System Architecture & Engineering Design

The application is engineered using a clean, decoupled 3-tier system architecture:

```text
===================================================================================
                       LAYER 1: QT 6 GRAPHICAL USER INTERFACE
===================================================================================
 [MainWindow]
  ├── Media Stream Inspector & Track Tree Manager
  ├── Output Container Format Selector (.mp4, .mkv, .webm, .avi, .mov, .flv, .ts, .mp3, .wav, .flac)
  ├── Resolution Preset Selector (4K, 2K, 1080p, 720p, 480p, 360p) & Custom Width/Height
  ├── External Track Addition (Audio: MP3/WAV/AAC; Subtitle: SRT/VTT/ASS)
  └── Telemetry, Progress Bar & Non-blocking User Controls
                                    │
                        Qt Signals / Slots (Thread-Safe)
                                    ▼
===================================================================================
                 LAYER 2: ASYNCHRONOUS THREADING & WORKER ORCHESTRATION
===================================================================================
 [ConversionWorker] (Runs inside dedicated QThread)
  ├── Primary Container Demuxer & Demux Loop
  ├── Secondary Demuxers for External Audio & Subtitle Files
  ├── Timestamp Offset Seeking (av_seek_frame) for Audio Start Delay
  ├── Duration Trimming & Cap Matching to Primary Video Length
  └── Progress Calculation & Error Reporting Signals
                                    │
                          C++ Direct Function Calls
                                    ▼
===================================================================================
                    LAYER 3: PURE C++ FFMPEG CORE ENGINE (0 Qt Dependencies)
===================================================================================
 [RAIIWrappers.h]       → Smart pointer deleters (AVFrame, AVPacket, AVFormatContext, AVAudioFifo)
 [TranscodeOptions.h]   → Unified transcode settings & container format default codecs
 [MediaDemuxer]         → Container probing, metadata extraction, stream discovery
 [MediaDecoder]         → Video/Audio packet decoding (avcodec_send_packet / avcodec_receive_frame)
 [AudioResampler]       → Audio format, sample rate & planar channel layout conversion (libswresample)
 [FrameScaler]          → Video resolution scaling & color conversion (libswscale)
 [MediaEncoder]         → AVAudioFifo 1024-sample frame buffering, H.264/VP9/AAC encoding & container muxing
===================================================================================
```

---

## 📁 Repository Structure

```text
Media_TranscodeX/
├── CMakeLists.txt               # Cross-platform CMake 3.16+ configuration file (desktop app)
├── README.md                    # Project documentation & architecture guide
├── Project_plan.md              # Feature development roadmap & sprint history
├── config.txt                   # Workspace configuration settings
├── server.py                    # Local dev server for the web app (CORS-enabled static file server)
├── src/                         # Desktop app (Qt 6 + FFmpeg C API)
│   ├── core/                    # Pure C++ FFmpeg Processing Engine (No Qt dependencies)
│   │   ├── RAIIWrappers.h       # C++17 smart pointer deleters (std::unique_ptr) for FFmpeg structs
│   │   ├── TranscodeOptions.h   # Transcode options struct & container codec auto-configuration
│   │   ├── MediaDemuxer.h       # Header for container demuxing & stream inspection
│   │   ├── MediaDemuxer.cpp     # Demuxer implementation (avformat_open_input, avfind_best_stream)
│   │   ├── MediaDecoder.h       # Header for video/audio decoding
│   │   ├── MediaDecoder.cpp     # Decoder implementation (avcodec_send_packet, avcodec_receive_frame)
│   │   ├── MediaEncoder.h       # Header for encoding, container muxing & AVAudioFifo buffering
│   │   ├── MediaEncoder.cpp     # Encoder implementation (avformat_write_header, AVAudioFifo, av_interleaved_write_frame)
│   │   ├── AudioResampler.h     # Header for audio resampling & format conversion
│   │   ├── AudioResampler.cpp   # Audio resampler implementation (SwrContext, swr_convert)
│   │   ├── FrameScaler.h        # Header for video frame scaling & color space conversion
│   │   └── FrameScaler.cpp      # Frame scaler implementation (SwsContext, sws_scale)
│   ├── worker/                  # Asynchronous Multithreaded Execution Layer
│   │   ├── ConversionWorker.h   # QThread worker header emitting progress & status signals
│   │   └── ConversionWorker.cpp # Multi-track demux, decode, scale, resample & mux pipeline
│   └── gui/                     # Desktop User Interface Layer (Qt 6 Widget Framework)
│       ├── main.cpp             # Application entry point & Qt main event loop
│       ├── MainWindow.h         # Main desktop dashboard header & UI layout slots
│       └── MainWindow.cpp       # Desktop UI implementation, track manager tree & transcode controls
└── web/                         # Web app (vanilla JS + ffmpeg.wasm, no build step)
    ├── index.html               # Page layout & vendored FFmpeg.wasm <script> includes
    ├── app.js                   # FFmpeg.wasm engine, transcode pipeline & UI wiring
    ├── styles.css               # Styling
    └── vendor/                  # Locally-hosted FFmpeg.wasm wrapper libraries (see below)
```

---

## ✨ Core Features & Technical Highlights

### 1. Fixed-Size Audio Frame Buffering (`AVAudioFifo`)
High-quality audio encoders like **AAC** require fixed frame sizes (typically **1024 samples** per frame). Input audio decoders and resamplers frequently yield variable frame sizes (e.g. 1152, 2048, or 4096 samples). 
- `MediaEncoder` incorporates FFmpeg's `AVAudioFifo` buffer to queue incoming samples.
- Audio is read out in exact 1024-sample blocks before being dispatched to `avcodec_send_frame()`.
- Flushes remaining leftover samples on stream closure to prevent silent audio output or packet drop errors.

### 2. Multi-Track Stream Management & External Track Multiplexing
- **Tree Inspector:** Probes container format metadata, duration, video resolution, frame rates, and all internal audio/subtitle tracks.
- **Selective Track Enable/Disable:** Checkboxes allow users to selectively exclude unwanted audio or subtitle tracks from the converted output.
- **External Track Injection:** Add external audio files (`.mp3`, `.wav`, `.aac`, `.flac`, `.ogg`) and subtitle files (`.srt`, `.vtt`, `.ass`) directly into the output container.

### 3. Audio Timing Offset & Duration Trimming
- **Start Time Offset:** Specify start delays (`startTimeSec`) for external audio tracks; `ConversionWorker` uses `av_seek_frame()` to start reading audio at the desired offset.
- **Auto Video Length Matching:** External audio processing automatically stops when it reaches the main video file's total duration ($X$ seconds) to prevent out-of-sync playback.

### 4. Custom Output Container Formats & Smart Codec Mapping
Supports converting to a wide variety of output container formats:
- **Video Containers:** MP4 (`.mp4`), Matroska (`.mkv`), WebM (`.webm`), AVI (`.avi`), QuickTime (`.mov`), Flash Video (`.flv`), MPEG-TS (`.ts`).
- **Audio-Only Containers:** MP3 (`.mp3`), WAV (`.wav`), FLAC (`.flac`).
- **Smart Codec Mapping:** Automatically configures optimal default video and audio codecs per container (e.g., H.264/AAC for MP4, VP9/OPUS for WebM, MPEG4/MP3 for AVI, PCM for WAV).

### 5. Resolution Scaling & Pixel Format Conversion
- **Resolution Selector:** Choose from standard resolution presets:
  - `3840 x 2160` (4K UHD)
  - `2560 x 1440` (2K QHD)
  - `1920 x 1080` (1080p Full HD)
  - `1280 x 720` (720p HD)
  - `854 x 480` (480p SD)
  - `640 x 360` (360p)
  - `Custom Resolution...` (Enables custom Width and Height spinbox inputs)
- **`FrameScaler` Engine:** Utilizes `libswscale` to perform high-speed bilinear pixel scaling and color space conversion (YUV420P).

---

## 📋 Prerequisites & Dependencies

To build and run **Media_TranscodeX**, ensure your Linux host environment has the following installed:

* **OS:** Linux (Ubuntu 20.04+ / Debian 11+ recommended)
* **Compiler:** C++17 compliant compiler (`g++` v9+ or `clang++` v10+)
* **Build System:** CMake (v3.16+)
* **Libraries:**
  * **Qt 6 Framework:** `qt6-base-dev`
  * **FFmpeg Libraries:** `libavcodec-dev`, `libavformat-dev`, `libavutil-dev`, `libswscale-dev`, `libswresample-dev`

### Installing Dependencies on Ubuntu / Debian

```bash
sudo apt update
sudo apt install -y \
    build-essential \
    cmake \
    qt6-base-dev \
    libavcodec-dev \
    libavformat-dev \
    libavutil-dev \
    libswscale-dev \
    libswresample-dev
```

---

## 💻 Build & Execution Guide

### 1. Clone & Build

```bash
# Clone the repository
git clone https://github.com/rakshi1701/MediaTranscodeX.git
cd Media_TranscodeX

# Create build directory and run CMake
mkdir -p build && cd build
cmake ..

# Build binary using all CPU cores
cmake --build . -j$(nproc)
```

### 2. Launch Application

```bash
./Media_TranscodeX
```

---

## 🌐 Web Version (Browser-Based)

The `web/` directory contains a completely independent implementation of the same converter, running entirely client-side in the browser via [ffmpeg.wasm](https://github.com/ffmpegwasm/ffmpeg.wasm) — no install, no server-side processing, and your media never leaves the browser. Deployed live via GitHub Pages: **https://rakshi1701.github.io/MediaTranscodeX/**

### Features
Mirrors the desktop app's core feature set: container/format conversion, resolution/bitrate/encoding-speed control, internal audio track selection, external audio/subtitle track injection with start-offset delay and duration trimming, and a live progress bar + log console.

### Architecture Notes
- **FFmpeg.wasm engine**: uses the single-threaded `@ffmpeg/core` build. A multi-threaded `core-mt` build was evaluated but reverted — it hung indefinitely mid-transcode in testing, a known issue with ffmpeg.wasm's nested pthread-worker model across browsers.
- **`web/vendor/`**: the `@ffmpeg/ffmpeg` and `@ffmpeg/util` wrapper libraries are vendored locally rather than loaded from a CDN. `@ffmpeg/ffmpeg` spins up its own internal Worker resolved relative to wherever its own script was loaded from, and browsers hard-block `new Worker()` on a cross-origin URL — vendoring keeps everything same-origin.
- **Integrity-verified core loading**: the actual `ffmpeg-core.js` / `ffmpeg-core.wasm` files are still fetched from a CDN at runtime (they're large, versioned binaries, not vendored), but `app.js` computes a SHA-384 digest of each and checks it against a pinned hash before use — CDN-tamper protection equivalent to Subresource Integrity, for files that can't use the `<script integrity>` attribute directly since they're loaded dynamically.
- **No cross-origin isolation required**: since the core is single-threaded, the app needs no `SharedArrayBuffer` / COOP / COEP setup — it works on any static file host, including GitHub Pages, with zero special server configuration.

### Run Locally

```bash
# From the repository root
python3 server.py

# Then open in your browser
http://localhost:8080
```

### Deployment
Pushing to the `web-version` or `Main` branch automatically deploys `web/` to GitHub Pages via `.github/workflows/deploy_web.yml`.

---

## 📄 License

This project is licensed under the **MIT License**.
