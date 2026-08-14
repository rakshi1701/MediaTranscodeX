#include "FrameScaler.h"
#include <iostream>

namespace Core {

FrameScaler::~FrameScaler() {
    if (m_swsContext) {
        sws_freeContext(m_swsContext);
        m_swsContext = nullptr;
    }
}

bool FrameScaler::init(int srcW, int srcH, AVPixelFormat srcFormat,
                        int dstW, int dstH, AVPixelFormat dstFormat) {
    m_srcWidth = srcW;
    m_srcHeight = srcH;
    m_srcFormat = srcFormat;
    m_dstWidth = dstW;
    m_dstHeight = dstH;
    m_dstFormat = dstFormat;

    // Re-create SwsContext with fast bilinear scaling
    m_swsContext = sws_getCachedContext(
        m_swsContext,
        m_srcWidth, m_srcHeight, m_srcFormat,
        m_dstWidth, m_dstHeight, m_dstFormat,
        SWS_BILINEAR, nullptr, nullptr, nullptr
    );

    if (!m_swsContext) {
        std::cerr << "[FrameScaler] Error: Failed to initialize SwsContext." << std::endl;
        return false;
    }

    return true;
}

QImage FrameScaler::scaleToQImage(const AVFrame* srcFrame) {
    if (!srcFrame || !m_swsContext) {
        return QImage();
    }

    // Allocate an output QImage initialized to RGB888 format
    QImage image(m_dstWidth, m_dstHeight, QImage::Format_RGB888);

    // Array pointers and line strides for destination buffer
    uint8_t* destData[1] = { image.bits() };
    int destLinesize[1] = { static_cast<int>(image.bytesPerLine()) };

    // Perform color space conversion & scale pixel dimensions
    sws_scale(
        m_swsContext,
        srcFrame->data,
        srcFrame->linesize,
        0,
        m_srcHeight,
        destData,
        destLinesize
    );

    return image;
}

FramePtr FrameScaler::scaleFrame(const AVFrame* srcFrame) {
    if (!srcFrame || !m_swsContext) return nullptr;

    FramePtr dstFrame(av_frame_alloc());
    if (!dstFrame) return nullptr;

    dstFrame->width = m_dstWidth;
    dstFrame->height = m_dstHeight;
    dstFrame->format = m_dstFormat;
    dstFrame->pts = srcFrame->pts;

    if (av_frame_get_buffer(dstFrame.get(), 32) < 0) {
        std::cerr << "[FrameScaler] Error: Failed to allocate destination AVFrame buffer." << std::endl;
        return nullptr;
    }

    sws_scale(
        m_swsContext,
        srcFrame->data,
        srcFrame->linesize,
        0,
        m_srcHeight,
        dstFrame->data,
        dstFrame->linesize
    );

    return dstFrame;
}

} // namespace Core