#ifndef AUDIO_RESAMPLER_H
#define AUDIO_RESAMPLER_H

#include "RAIIWrappers.h"

extern "C" {
    #include <libswresample/swresample.h>
    #include <libavutil/channel_layout.h>
}

namespace Core {

class AudioResampler {
public:
    AudioResampler() = default;
    ~AudioResampler();

    AudioResampler(const AudioResampler&) = delete;
    AudioResampler& operator=(const AudioResampler&) = delete;

    // Initialize resampler using legacy uint64_t channel layouts (FFmpeg 4.x/5.x compatible)
    bool init(uint64_t inChannelLayout, AVSampleFormat inFormat, int inSampleRate,
              uint64_t outChannelLayout, AVSampleFormat outFormat, int outSampleRate);

    // Resample an input AVFrame into a newly allocated output AVFrame
    FramePtr resampleFrame(const AVFrame* inFrame);

private:
    SwrContext* m_swrCtx = nullptr;
    uint64_t m_outChannelLayout = 0;
    AVSampleFormat m_outFormat = AV_SAMPLE_FMT_NONE;
    int m_outSampleRate = 0;
};

} // namespace Core

#endif // AUDIO_RESAMPLER_H