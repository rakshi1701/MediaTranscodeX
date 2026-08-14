#ifndef FRAME_SCALER_H
#define FRAME_SCALER_H

#include "RAIIWrappers.h"
#include <QImage>

extern "C" {
    #include <libswscale/swscale.h>
}

namespace Core {

class FrameScaler {
public:
    FrameScaler() = default;
    ~FrameScaler();

    FrameScaler(const FrameScaler&) = delete;
    FrameScaler& operator=(const FrameScaler&) = delete;

    // Initializes scaling context for target dimensions and pixel formats
    bool init(int srcW, int srcH, AVPixelFormat srcFormat,
              int dstW, int dstH, AVPixelFormat dstFormat = AV_PIX_FMT_RGB24);

    // Converts input AVFrame (e.g. YUV420p) to a Qt QImage (RGB24)
    QImage scaleToQImage(const AVFrame* srcFrame);

    // Rescales input AVFrame to target dimensions and pixel format
    FramePtr scaleFrame(const AVFrame* srcFrame);

private:
    SwsContext* m_swsContext = nullptr;
    int m_srcWidth = 0;
    int m_srcHeight = 0;
    AVPixelFormat m_srcFormat = AV_PIX_FMT_NONE;

    int m_dstWidth = 0;
    int m_dstHeight = 0;
    AVPixelFormat m_dstFormat = AV_PIX_FMT_RGB24;
};

} // namespace Core

#endif // FRAME_SCALER_H