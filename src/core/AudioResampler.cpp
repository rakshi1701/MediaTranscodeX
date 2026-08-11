#include "AudioResampler.h"
#include <iostream>

namespace Core {

AudioResampler::~AudioResampler() {
    if (m_swrCtx) {
        swr_free(&m_swrCtx);
    }
    av_channel_layout_uninit(&m_outLayout);
}

bool AudioResampler::init(const AVChannelLayout* inLayout, AVSampleFormat inFormat, int inSampleRate,
                          const AVChannelLayout* outLayout, AVSampleFormat outFormat, int outSampleRate) {
    if (!inLayout || !outLayout) return false;

    av_channel_layout_copy(&m_outLayout, outLayout);
    m_outFormat = outFormat;
    m_outSampleRate = outSampleRate;

    // Allocate SwrContext
    int ret = swr_alloc_set_opts2(
        &m_swrCtx,
        &m_outLayout, m_outFormat, m_outSampleRate,
        inLayout, inFormat, inSampleRate,
        0, nullptr
    );

    if (ret < 0 || !m_swrCtx) {
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

    // Calculate maximum output sample count with delay buffer
    int64_t delay = swr_get_delay(m_swrCtx, inFrame->sample_rate);
    int outSamples = static_cast<int>(av_rescale_rnd(
        delay + inFrame->nb_samples,
        m_outSampleRate,
        inFrame->sample_rate,
        AV_ROUND_UP
    ));

    outFrame->sample_rate = m_outSampleRate;
    outFrame->format = m_outFormat;
    av_channel_layout_copy(&outFrame->ch_layout, &m_outLayout);
    outFrame->nb_samples = outSamples;

    if (av_frame_get_buffer(outFrame.get(), 0) < 0) {
        std::cerr << "[AudioResampler] Error: Failed to allocate frame buffer." << std::endl;
        return nullptr;
    }

    // Perform conversion
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
