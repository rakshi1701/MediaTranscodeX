#include "MediaDecoder.h"
#include <iostream>

namespace Core {

bool MediaDecoder::init(AVStream* stream) {
    if (!stream) {
        std::cerr << "[MediaDecoder] Error: Provided AVStream is null." << std::endl;
        return false;
    }

    // 1. Find decoder for codec ID
    const AVCodec* codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!codec) {
        std::cerr << "[MediaDecoder] Error: Unsupported codec ID." << std::endl;
        return false;
    }

    // 2. Allocate codec context
    AVCodecContext* rawCtx = avcodec_alloc_context3(codec);
    if (!rawCtx) {
        std::cerr << "[MediaDecoder] Error: Failed to allocate AVCodecContext." << std::endl;
        return false;
    }

    // Wrap raw pointer into RAII smart pointer
    m_codecContext.reset(rawCtx);

    // 3. Copy parameters from stream to codec context
    if (avcodec_parameters_to_context(m_codecContext.get(), stream->codecpar) < 0) {
        std::cerr << "[MediaDecoder] Error: Failed to copy parameters to codec context." << std::endl;
        return false;
    }

    // 4. Open decoder
    if (avcodec_open2(m_codecContext.get(), codec, nullptr) < 0) {
        std::cerr << "[MediaDecoder] Error: Failed to open codec." << std::endl;
        return false;
    }

    m_streamIndex = stream->index;
    return true;
}

bool MediaDecoder::decodePacket(AVPacket* packet, const std::function<void(AVFrame*)>& frameHandler) {
    if (!m_codecContext) return false;

    // Send packet to decoder
    int ret = avcodec_send_packet(m_codecContext.get(), packet);
    if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
        std::cerr << "[MediaDecoder] Error sending packet to decoder." << std::endl;
        return false;
    }

    // Receive raw frames loop
    while (ret >= 0) {
        FramePtr frame(av_frame_alloc());
        if (!frame) break;

        ret = avcodec_receive_frame(m_codecContext.get(), frame.get());
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break; // Needs more packets or EOF
        } else if (ret < 0) {
            std::cerr << "[MediaDecoder] Error receiving decoded frame." << std::endl;
            return false;
        }

        // Pass raw frame to callback (automatically freed when FramePtr goes out of scope)
        if (frameHandler) {
            frameHandler(frame.get());
        }
    }

    return true;
}

bool MediaDecoder::flush(const std::function<void(AVFrame*)>& frameHandler) {
    // Passing nullptr packet drains remaining frames buffered inside decoder
    return decodePacket(nullptr, frameHandler);
}

} // namespace Core