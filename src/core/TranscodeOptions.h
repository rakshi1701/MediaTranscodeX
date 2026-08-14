#ifndef TRANSCODE_OPTIONS_H
#define TRANSCODE_OPTIONS_H

#include <string>
#include <vector>

extern "C" {
    #include <libavcodec/avcodec.h>
}

namespace Core {

enum class RateControlMode {
    Bitrate,
    CRF
};

enum class TrackSourceType {
    Internal,
    External
};

enum class TrackType {
    Video,
    Audio,
    Subtitle
};

struct TrackSelection {
    TrackType type = TrackType::Audio;
    TrackSourceType sourceType = TrackSourceType::Internal;
    int sourceStreamIndex = -1;       // Stream index in internal container
    std::string externalFilePath = ""; // Absolute file path if external file
    bool enabled = true;
    std::string language = "";
    std::string title = "";
    
    double startTimeSec = 0.0;     // Start offset within external media file
    double maxDurationSec = 0.0;   // Max duration in seconds (0.0 = match video duration)
};

// --- Video Options ---
struct VideoOptions {
    bool enableVideo = true;
    AVCodecID codecId = AV_CODEC_ID_H264;
    
    int targetWidth = 0;   // 0 = preserve original
    int targetHeight = 0;  // 0 = preserve original
    double targetFps = 0.0; // 0.0 = preserve original

    RateControlMode rateMode = RateControlMode::CRF;
    int bitRate = 4000000;
    int crf = 23;
    std::string preset = "medium";
    AVPixelFormat pixFmt = AV_PIX_FMT_YUV420P;
};

// --- Audio Options ---
struct AudioOptions {
    bool enableAudio = true;
    AVCodecID codecId = AV_CODEC_ID_AAC;
    int bitRate = 192000;      // 192 kbps default
    int sampleRate = 48000;    // 48 kHz default
    int channels = 2;          // Stereo default
    AVSampleFormat sampleFmt = AV_SAMPLE_FMT_FLTP; // Planar float for AAC
};

// --- Unified Transcode Configuration ---
struct TranscodeOptions {
    std::string inputFilePath;
    std::string outputFilePath;
    
    VideoOptions video;
    AudioOptions audio;

    std::vector<TrackSelection> selectedAudioTracks;
    std::vector<TrackSelection> selectedSubtitleTracks;
};

} // namespace Core

#endif // TRANSCODE_OPTIONS_H