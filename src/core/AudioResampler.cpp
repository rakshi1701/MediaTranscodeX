#include "AudioResampler.h"
#include <iostream>

namespace Core {

AudioResampler::~AudioResampler() {
    if (m_swrCtx) {
        swr_free(&m_swrCtx);
    }
}

bool AudioResampler::init(uint64_t inChannelLayout, AVSampleFormat inFormat, int inSampleRate,
                          uint64_t outChannelLayout, AVSampleFormat outFormat, int outSampleRate) {
    
    // Default to stereo if input layout is undefined
    if (inChannelLayout == 0) {
        inChannelLayout = AV_CH_LAYOUT_STEREO;
    }
    if (outChannelLayout == 0) {
        outChannelLayout = AV_CH_LAYOUT_STEREO;
    }

    m_outChannelLayout = outChannelLayout;
    m_outFormat = outFormat;
    m_outSampleRate = outSampleRate;

    // Use swr_alloc_set_opts for backwards compatibility with FFmpeg 4.x
    m_swrCtx = swr_alloc_set_opts(
        nullptr,
        m_outChannelLayout, m_outFormat, m_outSampleRate,
        inChannelLayout, inFormat, inSampleRate,
        0, nullptr
    );

    if (!m_swrCtx) {
        std::cerr << "[AudioResampler] Error: Failed to allocate SwrContext." << std::endl;
        return false;
    }

    if (swr_init(m_swrCtx) < 0) {
        std::cerr << "[AudioResampler] Error: Failed to initialize SwrContext." << std::endl;
        return false;
    }

    return true;
}

FramePtr AudioResampler::resampleFrame(const AVFrame* inFrame) {
    if (!inFrame || !m_swrCtx) return nullptr;

    FramePtr outFrame(av_frame_alloc());
    if (!outFrame) return nullptr;

    int64_t delay = swr_get_delay(m_swrCtx, inFrame->sample_rate);
    int outSamples = static_cast<int>(av_rescale_rnd(
        delay + inFrame->nb_samples,
        m_outSampleRate,
        inFrame->sample_rate,
        AV_ROUND_UP
    ));

    outFrame->sample_rate = m_outSampleRate;
    outFrame->format = m_outFormat;
    outFrame->channel_layout = m_outChannelLayout;
    outFrame->nb_samples = outSamples;

    if (av_frame_get_buffer(outFrame.get(), 0) < 0) {
        std::cerr << "[AudioResampler] Error: Failed to allocate frame buffer." << std::endl;
        return nullptr;
    }

    int convertedSamples = swr_convert(
        m_swrCtx,
        outFrame->data,
        outSamples,
        (const uint8_t**)inFrame->data,
        inFrame->nb_samples
    );

    if (convertedSamples < 0) {
        std::cerr << "[AudioResampler] Error during audio resampling." << std::endl;
        return nullptr;
    }

    outFrame->nb_samples = convertedSamples;
    outFrame->pts = av_rescale_q(inFrame->pts, AVRational{1, inFrame->sample_rate}, AVRational{1, m_outSampleRate});

    return outFrame;
}

} // namespace Core