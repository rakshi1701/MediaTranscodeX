#ifndef MEDIA_ENCODER_H
#define MEDIA_ENCODER_H

#include "RAIIWrappers.h"
#include "TranscodeOptions.h"

namespace Core {

class MediaEncoder {
public:
    MediaEncoder() = default;
    ~MediaEncoder();

    MediaEncoder(const MediaEncoder&) = delete;
    MediaEncoder& operator=(const MediaEncoder&) = delete;

    // Initialize container with video and audio options
    bool init(const TranscodeOptions& options, bool hasVideo, bool hasAudio);

    bool encodeVideoFrame(AVFrame* frame);
    bool encodeAudioFrame(AVFrame* frame);

    bool finish();

    AVCodecContext* getVideoCodecContext() const { return m_videoCodecCtx; }
    AVCodecContext* getAudioCodecContext() const { return m_audioCodecCtx; }

private:
    bool writePacket(AVPacket* pkt, AVRational timeBase, AVStream* stream);

    AVFormatContext* m_outputFormatCtx = nullptr;

    // Video Stream State
    AVCodecContext* m_videoCodecCtx = nullptr;
    AVStream* m_videoStream = nullptr;
    int64_t m_nextVideoPts = 0;

    // Audio Stream State
    AVCodecContext* m_audioCodecCtx = nullptr;
    AVStream* m_audioStream = nullptr;
    int64_t m_nextAudioPts = 0;

    bool m_headerWritten = false;
    bool m_finished = false;
};

} // namespace Core

#endif // MEDIA_ENCODER_H