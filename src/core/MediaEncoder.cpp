#include "MediaEncoder.h"
#include <iostream>

extern "C" {
    #include <libavutil/channel_layout.h>
}

namespace Core {

MediaEncoder::~MediaEncoder() {
    if (!m_finished) {
        finish();
    }
}

bool MediaEncoder::init(const TranscodeOptions& options, bool hasVideo, bool hasAudio) {
    // 1. Allocate output context
    int ret = avformat_alloc_output_context2(&m_outputFormatCtx, nullptr, nullptr, options.outputFilePath.c_str());
    if (ret < 0 || !m_outputFormatCtx) {
        std::cerr << "[MediaEncoder] Error: Could not allocate output context for file: " << options.outputFilePath << std::endl;
        return false;
    }

    // 2. Setup Video Stream
    if (hasVideo && options.video.enableVideo) {
        const AVCodec* videoCodec = avcodec_find_encoder(options.video.codecId);
        if (!videoCodec) {
            std::cerr << "[MediaEncoder] Error: Video encoder not found." << std::endl;
            return false;
        }

        m_videoStream = avformat_new_stream(m_outputFormatCtx, nullptr);
        m_videoCodecCtx = avcodec_alloc_context3(videoCodec);

        m_videoCodecCtx->width  = (options.video.targetWidth  % 2 == 0) ? options.video.targetWidth  : options.video.targetWidth - 1;
        m_videoCodecCtx->height = (options.video.targetHeight % 2 == 0) ? options.video.targetHeight : options.video.targetHeight - 1;
        m_videoCodecCtx->pix_fmt = options.video.pixFmt;
        m_videoCodecCtx->bit_rate = options.video.bitRate;
        
        int fps = (options.video.targetFps > 0) ? static_cast<int>(options.video.targetFps) : 30;
        m_videoCodecCtx->time_base = AVRational{1, fps};
        m_videoStream->time_base = m_videoCodecCtx->time_base;

        if (m_outputFormatCtx->oformat->flags & AVFMT_GLOBALHEADER) {
            m_videoCodecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        }

        if (avcodec_open2(m_videoCodecCtx, videoCodec, nullptr) < 0) {
            std::cerr << "[MediaEncoder] Error: Failed to open video codec." << std::endl;
            return false;
        }

        avcodec_parameters_from_context(m_videoStream->codecpar, m_videoCodecCtx);
    }

    // 3. Setup Audio Stream
    if (hasAudio && options.audio.enableAudio) {
        const AVCodec* audioCodec = avcodec_find_encoder(options.audio.codecId);
        if (!audioCodec) {
            std::cerr << "[MediaEncoder] Error: Audio encoder not found." << std::endl;
            return false;
        }

        m_audioStream = avformat_new_stream(m_outputFormatCtx, nullptr);
        m_audioCodecCtx = avcodec_alloc_context3(audioCodec);

        m_audioCodecCtx->sample_rate = options.audio.sampleRate;
        m_audioCodecCtx->sample_fmt = options.audio.sampleFmt;
        m_audioCodecCtx->bit_rate = options.audio.bitRate;
        
        // av_channel_layout_default(&m_audioCodecCtx->ch_layout, options.audio.channels);

        m_audioCodecCtx->channel_layout = av_get_default_channel_layout(options.audio.channels);
        m_audioCodecCtx->channels = options.audio.channels;
        
        m_audioCodecCtx->time_base = AVRational{1, options.audio.sampleRate};
        m_audioStream->time_base = m_audioCodecCtx->time_base;

        if (m_outputFormatCtx->oformat->flags & AVFMT_GLOBALHEADER) {
            m_audioCodecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        }

        if (avcodec_open2(m_audioCodecCtx, audioCodec, nullptr) < 0) {
            std::cerr << "[MediaEncoder] Error: Failed to open audio codec." << std::endl;
            return false;
        }

        avcodec_parameters_from_context(m_audioStream->codecpar, m_audioCodecCtx);
    }

    // 4. Open File & Write Header
    if (!(m_outputFormatCtx->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&m_outputFormatCtx->pb, options.outputFilePath.c_str(), AVIO_FLAG_WRITE) < 0) {
            std::cerr << "[MediaEncoder] Error: Could not open file for writing." << std::endl;
            return false;
        }
    }

    if (avformat_write_header(m_outputFormatCtx, nullptr) < 0) {
        std::cerr << "[MediaEncoder] Error: Could not write container header." << std::endl;
        return false;
    }

    m_headerWritten = true;
    return true;
}

bool MediaEncoder::encodeVideoFrame(AVFrame* frame) {
    if (!m_videoCodecCtx) return false;

    if (frame) {
        frame->pts = m_nextVideoPts++;
    }

    int ret = avcodec_send_frame(m_videoCodecCtx, frame);
    if (ret < 0) return false;

    while (ret >= 0) {
        PacketPtr pkt(av_packet_alloc());
        if (!pkt) break;

        ret = avcodec_receive_packet(m_videoCodecCtx, pkt.get());
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
        if (ret < 0) return false;

        writePacket(pkt.get(), m_videoCodecCtx->time_base, m_videoStream);
    }

    return true;
}

bool MediaEncoder::encodeAudioFrame(AVFrame* frame) {
    if (!m_audioCodecCtx) return false;

    if (frame) {
        frame->pts = m_nextAudioPts;
        m_nextAudioPts += frame->nb_samples;
    }

    int ret = avcodec_send_frame(m_audioCodecCtx, frame);
    if (ret < 0) return false;

    while (ret >= 0) {
        PacketPtr pkt(av_packet_alloc());
        if (!pkt) break;

        ret = avcodec_receive_packet(m_audioCodecCtx, pkt.get());
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
        if (ret < 0) return false;

        writePacket(pkt.get(), m_audioCodecCtx->time_base, m_audioStream);
    }

    return true;
}

bool MediaEncoder::writePacket(AVPacket* pkt, AVRational timeBase, AVStream* stream) {
    if (!m_outputFormatCtx || !m_headerWritten || !stream) return false;

    av_packet_rescale_ts(pkt, timeBase, stream->time_base);
    pkt->stream_index = stream->index;

    return av_interleaved_write_frame(m_outputFormatCtx, pkt) >= 0;
}

bool MediaEncoder::finish() {
    if (m_finished) return true;

    if (m_videoCodecCtx) encodeVideoFrame(nullptr);
    if (m_audioCodecCtx) encodeAudioFrame(nullptr);

    if (m_outputFormatCtx && m_headerWritten) {
        av_write_trailer(m_outputFormatCtx);
    }

    if (m_videoCodecCtx) avcodec_free_context(&m_videoCodecCtx);
    if (m_audioCodecCtx) avcodec_free_context(&m_audioCodecCtx);

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