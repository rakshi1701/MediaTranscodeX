#ifndef MEDIA_ENCODER_H
#define MEDIA_ENCODER_H

#include "RAIIWrappers.h"
#include "TranscodeOptions.h"
#include <vector>

extern "C" {
    #include <libavutil/audio_fifo.h>
}

namespace Core {

struct AudioStreamState {
    AVCodecContext* codecCtx = nullptr;
    AVStream* stream = nullptr;
    AVAudioFifo* fifo = nullptr;
    int64_t nextPts = 0;
};

struct SubtitleStreamState {
    AVStream* stream = nullptr;
};

class MediaEncoder {
public:
    MediaEncoder() = default;
    ~MediaEncoder();

    MediaEncoder(const MediaEncoder&) = delete;
    MediaEncoder& operator=(const MediaEncoder&) = delete;

    // Initialize container with video, multiple audio options, and subtitle options
    bool init(const TranscodeOptions& options, bool hasVideo, size_t numAudioTracks, size_t numSubtitleTracks);

    bool encodeVideoFrame(AVFrame* frame);
    bool encodeAudioFrame(size_t trackIdx, AVFrame* frame);
    bool writeSubtitlePacket(size_t trackIdx, AVPacket* pkt, AVRational srcTimeBase);

    bool finish();

    AVCodecContext* getVideoCodecContext() const { return m_videoCodecCtx; }
    AVCodecContext* getAudioCodecContext(size_t trackIdx = 0) const {
        return (trackIdx < m_audioStreams.size()) ? m_audioStreams[trackIdx].codecCtx : nullptr;
    }

private:
    bool writePacket(AVPacket* pkt, AVRational timeBase, AVStream* stream);

    AVFormatContext* m_outputFormatCtx = nullptr;

    // Video Stream State
    AVCodecContext* m_videoCodecCtx = nullptr;
    AVStream* m_videoStream = nullptr;
    int64_t m_nextVideoPts = 0;

    // Multi-Audio Stream State
    std::vector<AudioStreamState> m_audioStreams;

    // Multi-Subtitle Stream State
    std::vector<SubtitleStreamState> m_subtitleStreams;

    bool m_headerWritten = false;
    bool m_finished = false;
};

} // namespace Core

#endif // MEDIA_ENCODER_H