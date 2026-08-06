#ifndef RAII_WRAPPERS_H
#define RAII_WRAPPERS_H

#include <memory>

extern "C" {
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
    #include <libavutil/avutil.h>
}

namespace Core {

// Deleter for AVFormatContext (Input container)
struct AVFormatContextDeleter {
    void operator()(AVFormatContext* ctx) const {
        if (ctx) {
            avformat_close_input(&ctx);
        }
    }
};

// Deleter for AVCodecContext
struct AVCodecContextDeleter {
    void operator()(AVCodecContext* ctx) const {
        if (ctx) {
            avcodec_free_context(&ctx);
        }
    }
};

// Deleter for AVPacket (Encoded data packet)
struct AVPacketDeleter {
    void operator()(AVPacket* pkt) const {
        if (pkt) {
            av_packet_free(&pkt);
        }
    }
};

// Deleter for AVFrame (Raw uncompressed frame)
struct AVFrameDeleter {
    void operator()(AVFrame* frame) const {
        if (frame) {
            av_frame_free(&frame);
        }
    }
};

// --- Type Aliases for Clean Modern C++ Code ---
using FormatContextPtr = std::unique_ptr<AVFormatContext, AVFormatContextDeleter>;
using CodecContextPtr  = std::unique_ptr<AVCodecContext, AVCodecContextDeleter>;
using PacketPtr        = std::unique_ptr<AVPacket, AVPacketDeleter>;
using FramePtr         = std::unique_ptr<AVFrame, AVFrameDeleter>;

} // namespace Core

#endif // RAII_WRAPPERS_H