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

    // Initialize resampler context
    bool init(const AVChannelLayout* inLayout, AVSampleFormat inFormat, int inSampleRate,
              const AVChannelLayout* outLayout, AVSampleFormat outFormat, int outSampleRate);

    // Resample an input AVFrame into a newly allocated output AVFrame
    FramePtr resampleFrame(const AVFrame* inFrame);

private:
    SwrContext* m_swrCtx = nullptr;
    AVChannelLayout m_outLayout{};
    AVSampleFormat m_outFormat = AV_SAMPLE_FMT_NONE;
    int m_outSampleRate = 0;
};

} // namespace Core

#endif // AUDIO_RESAMPLER_H