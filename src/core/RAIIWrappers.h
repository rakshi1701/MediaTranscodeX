#ifndef RAII_WRAPPERS_H
#define RAII_WRAPPERS_H

#include <memory>

extern "C" {
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
    #include <libavutil/avutil.h>
    #include <libavutil/channel_layout.h>
    #include <libavutil/version.h>
}

namespace Core {

namespace Compat {

// Helper to get number of channels from AVCodecParameters
inline int getChannels(const AVCodecParameters* par) {
    if (!par) return 0;
#if LIBAVUTIL_VERSION_MAJOR >= 57
    return par->ch_layout.nb_channels;
#else
    return par->channels;
#endif
}

// Helper to get number of channels from AVCodecContext
inline int getChannels(const AVCodecContext* ctx) {
    if (!ctx) return 0;
#if LIBAVUTIL_VERSION_MAJOR >= 57
    return ctx->ch_layout.nb_channels;
#else
    return ctx->channels;
#endif
}

// Helper to get channel layout mask from AVCodecContext
inline uint64_t getChannelLayout(const AVCodecContext* ctx) {
    if (!ctx) return AV_CH_LAYOUT_STEREO;
#if LIBAVUTIL_VERSION_MAJOR >= 57
    if (ctx->ch_layout.order == AV_CHANNEL_ORDER_NATIVE) {
        return ctx->ch_layout.u.mask;
    }
    return av_get_default_channel_layout(ctx->ch_layout.nb_channels);
#else
    if (ctx->channel_layout) {
        return ctx->channel_layout;
    }
    return av_get_default_channel_layout(ctx->channels);
#endif
}

// Helper to set channels and channel layout on AVCodecContext
inline void setAudioLayout(AVCodecContext* ctx, int numChannels) {
    if (!ctx) return;
#if LIBAVUTIL_VERSION_MAJOR >= 57
    av_channel_layout_default(&ctx->ch_layout, numChannels);
#else
    ctx->channels = numChannels;
    ctx->channel_layout = av_get_default_channel_layout(numChannels);
#endif
}

// Helper to set channels and channel layout on AVFrame
inline void setFrameAudioLayout(AVFrame* frame, const AVCodecContext* encCtx) {
    if (!frame || !encCtx) return;
#if LIBAVUTIL_VERSION_MAJOR >= 57
    av_channel_layout_copy(&frame->ch_layout, &encCtx->ch_layout);
#else
    frame->channel_layout = encCtx->channel_layout;
    frame->channels = encCtx->channels;
#endif
}

// Helper to set channel layout mask on AVFrame
inline void setFrameAudioLayoutMask(AVFrame* frame, uint64_t mask) {
    if (!frame) return;
#if LIBAVUTIL_VERSION_MAJOR >= 57
    av_channel_layout_from_mask(&frame->ch_layout, mask);
#else
    frame->channel_layout = mask;
#endif
}

} // namespace Compat

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