#include "MediaEncoder.h"
#include <iostream>

namespace Core {

MediaEncoder::~MediaEncoder() {
    if (!m_finished) {
        finish();
    }
}

bool MediaEncoder::init(const EncoderConfig& config) {
    // 1. Allocate output format context
    int ret = avformat_alloc_output_context2(&m_outputFormatCtx, nullptr, nullptr, config.outputFilePath.c_str());
    if (ret < 0 || !m_outputFormatCtx) {
        std::cerr << "[MediaEncoder] Error: Could not deduce output format from file extension." << std::endl;
        return false;
    }

    // 2. Find video encoder
    const AVCodec* codec = avcodec_find_encoder(config.codecId);
    if (!codec) {
        std::cerr << "[MediaEncoder] Error: Encoder codec not found." << std::endl;
        return false;
    }

    // 3. Create video stream inside output container
    m_videoStream = avformat_new_stream(m_outputFormatCtx, nullptr);
    if (!m_videoStream) {
        std::cerr << "[MediaEncoder] Error: Could not create output stream." << std::endl;
        return false;
    }

    // 4. Allocate and setup codec context
    m_videoCodecCtx = avcodec_alloc_context3(codec);
    if (!m_videoCodecCtx) {
        std::cerr << "[MediaEncoder] Error: Could not allocate codec context." << std::endl;
        return false;
    }

    // m_videoCodecCtx->height = config.height;
    // m_videoCodecCtx->width = config.width;

    // Ensure width and height are divisible by 2 for H.264 / yuv420p compatibility
    m_videoCodecCtx->width  = (config.width  % 2 == 0) ? config.width  : config.width - 1;
    m_videoCodecCtx->height = (config.height % 2 == 0) ? config.height : config.height - 1;
    m_videoCodecCtx->pix_fmt = config.pixFmt;
    m_videoCodecCtx->bit_rate = config.bitRate;
    m_videoCodecCtx->time_base = AVRational{1, config.framerate};
    m_videoStream->time_base = m_videoCodecCtx->time_base;

    // Set flag for global headers (required for containers like MP4)
    if (m_outputFormatCtx->oformat->flags & AVFMT_GLOBALHEADER) {
        m_videoCodecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    // 5. Open encoder
    if (avcodec_open2(m_videoCodecCtx, codec, nullptr) < 0) {
        std::cerr << "[MediaEncoder] Error: Could not open video encoder." << std::endl;
        return false;
    }

    // Copy parameters from codec context to output stream
    if (avcodec_parameters_from_context(m_videoStream->codecpar, m_videoCodecCtx) < 0) {
        std::cerr << "[MediaEncoder] Error: Could not copy codec params to stream." << std::endl;
        return false;
    }

    // 6. Open output file for writing
    if (!(m_outputFormatCtx->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&m_outputFormatCtx->pb, config.outputFilePath.c_str(), AVIO_FLAG_WRITE) < 0) {
            std::cerr << "[MediaEncoder] Error: Could not open output file: " << config.outputFilePath << std::endl;
            return false;
        }
    }

    // 7. Write container header
    if (avformat_write_header(m_outputFormatCtx, nullptr) < 0) {
        std::cerr << "[MediaEncoder] Error: Could not write container header." << std::endl;
        return false;
    }

    m_headerWritten = true;
    m_nextPts = 0;
    return true;
}

bool MediaEncoder::encodeVideoFrame(AVFrame* frame) {
    if (!m_videoCodecCtx) return false;

    if (frame) {
        frame->pts = m_nextPts++;
    }

    // Send frame to encoder
    int ret = avcodec_send_frame(m_videoCodecCtx, frame);
    if (ret < 0) {
        std::cerr << "[MediaEncoder] Error sending frame for encoding." << std::endl;
        return false;
    }

    // Receive packets from encoder loop
    while (ret >= 0) {
        PacketPtr pkt(av_packet_alloc());
        if (!pkt) break;

        ret = avcodec_receive_packet(m_videoCodecCtx, pkt.get());
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            std::cerr << "[MediaEncoder] Error receiving packet from encoder." << std::endl;
            return false;
        }

        // Rescale packet timestamps from codec time_base to stream time_base
        av_packet_rescale_ts(pkt.get(), m_videoCodecCtx->time_base, m_videoStream->time_base);
        pkt->stream_index = m_videoStream->index;

        if (!writePacket(pkt.get())) {
            return false;
        }
    }

    return true;
}

bool MediaEncoder::writePacket(AVPacket* pkt) {
    if (!m_outputFormatCtx || !m_headerWritten) return false;
    return av_interleaved_write_frame(m_outputFormatCtx, pkt) >= 0;
}

bool MediaEncoder::finish() {
    if (m_finished) return true;

    // Flush encoder by passing nullptr frame
    if (m_videoCodecCtx) {
        encodeVideoFrame(nullptr);
    }

    // Write container trailer
    if (m_outputFormatCtx && m_headerWritten) {
        av_write_trailer(m_outputFormatCtx);
    }

    // Free resources
    if (m_videoCodecCtx) {
        avcodec_free_context(&m_videoCodecCtx);
    }

    if (m_outputFormatCtx) {
        if (!(m_outputFormatCtx->oformat->flags & AVFMT_NOFILE) && m_outputFormatCtx->pb) {
            avio_closep(&m_outputFormatCtx->pb);
        }
        avformat_free_context(m_outputFormatCtx);
        m_outputFormatCtx = nullptr;
    }

    m_finished = true;
    return true;
}

} // namespace Core