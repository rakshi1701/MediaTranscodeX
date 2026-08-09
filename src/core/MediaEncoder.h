#ifndef MEDIA_ENCODER_H
#define MEDIA_ENCODER_H

#include "RAIIWrappers.h"
#include <string>

namespace Core {

struct EncoderConfig {
    std::string outputFilePath;
    int width = 1280;
    int height = 720;
    int bitRate = 2000000; // 2 Mbps
    int framerate = 30;
    AVPixelFormat pixFmt = AV_PIX_FMT_YUV420P;
    AVCodecID codecId = AV_CODEC_ID_H264;
};

class MediaEncoder {
public:
    MediaEncoder() = default;
    ~MediaEncoder();

    MediaEncoder(const MediaEncoder&) = delete;
    MediaEncoder& operator=(const MediaEncoder&) = delete;

    // Initializes output container and video codec context
    bool init(const EncoderConfig& config);

    // Encodes a raw video AVFrame into an output packet and writes it to container
    bool encodeVideoFrame(AVFrame* frame);

    // Flushes buffered packets and closes container trailer
    bool finish();

private:
    bool writePacket(AVPacket* pkt);

    AVFormatContext* m_outputFormatCtx = nullptr;
    AVCodecContext* m_videoCodecCtx = nullptr;
    AVStream* m_videoStream = nullptr;
    int64_t m_nextPts = 0;
    bool m_headerWritten = false;
    bool m_finished = false;
};

} // namespace Core

#endif // MEDIA_ENCODER_H