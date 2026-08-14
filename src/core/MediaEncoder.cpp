#include "MediaEncoder.h"
#include <iostream>

extern "C" {
    #include <libavutil/channel_layout.h>
    #include <libavutil/opt.h>
}

namespace Core {

MediaEncoder::~MediaEncoder() {
    if (!m_finished) {
        finish();
    }
}

bool MediaEncoder::init(const TranscodeOptions& options, bool hasVideo, size_t numAudioTracks, size_t numSubtitleTracks) {
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

    // 3. Setup Audio Streams
    if (options.audio.enableAudio) {
        const AVCodec* audioCodec = avcodec_find_encoder(options.audio.codecId);
        if (audioCodec) {
            for (size_t i = 0; i < numAudioTracks; ++i) {
                AudioStreamState aState;
                aState.stream = avformat_new_stream(m_outputFormatCtx, nullptr);
                aState.codecCtx = avcodec_alloc_context3(audioCodec);

                aState.codecCtx->sample_rate = options.audio.sampleRate;
                aState.codecCtx->sample_fmt = options.audio.sampleFmt;
                aState.codecCtx->bit_rate = options.audio.bitRate;
                Core::Compat::setAudioLayout(aState.codecCtx, options.audio.channels);
                
                aState.codecCtx->time_base = AVRational{1, options.audio.sampleRate};
                aState.stream->time_base = aState.codecCtx->time_base;

                if (m_outputFormatCtx->oformat->flags & AVFMT_GLOBALHEADER) {
                    aState.codecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
                }

                if (avcodec_open2(aState.codecCtx, audioCodec, nullptr) < 0) {
                    std::cerr << "[MediaEncoder] Error: Failed to open audio codec for track " << i << std::endl;
                    return false;
                }

                aState.fifo = av_audio_fifo_alloc(aState.codecCtx->sample_fmt, Core::Compat::getChannels(aState.codecCtx), 1);
                if (!aState.fifo) {
                    std::cerr << "[MediaEncoder] Error: Could not allocate AVAudioFifo for track " << i << std::endl;
                    return false;
                }

                avcodec_parameters_from_context(aState.stream->codecpar, aState.codecCtx);
                m_audioStreams.push_back(aState);
            }
        }
    }

    // 4. Setup Subtitle Streams
    for (size_t j = 0; j < numSubtitleTracks; ++j) {
        SubtitleStreamState sState;
        sState.stream = avformat_new_stream(m_outputFormatCtx, nullptr);
        
        // Pick appropriate subtitle codec based on output container
        AVCodecID subCodecId = AV_CODEC_ID_SUBRIP;
        if (m_outputFormatCtx->oformat && std::string(m_outputFormatCtx->oformat->name).find("mp4") != std::string::npos) {
            subCodecId = AV_CODEC_ID_MOV_TEXT;
        }

        sState.stream->codecpar->codec_type = AVMEDIA_TYPE_SUBTITLE;
        sState.stream->codecpar->codec_id = subCodecId;

        m_subtitleStreams.push_back(sState);
    }

    // 5. Open File & Write Header
    if (!(m_outputFormatCtx->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&m_outputFormatCtx->pb, options.outputFilePath.c_str(), AVIO_FLAG_WRITE) < 0) {
            std::cerr << "[MediaEncoder] Error: Could not open file for writing." << std::endl;
            return false;
        }
    }

    if (m_outputFormatCtx->priv_data) {
        av_opt_set(m_outputFormatCtx->priv_data, "avoid_negative_ts", "make_zero", 0);
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

bool MediaEncoder::encodeAudioFrame(size_t trackIdx, AVFrame* frame) {
    if (trackIdx >= m_audioStreams.size()) return false;
    AudioStreamState& aState = m_audioStreams[trackIdx];
    if (!aState.codecCtx || !aState.fifo) return false;

    if (frame && frame->nb_samples > 0) {
        int fifoRet = av_audio_fifo_realloc(aState.fifo, av_audio_fifo_size(aState.fifo) + frame->nb_samples);
        (void)fifoRet;
        av_audio_fifo_write(aState.fifo, (void**)frame->extended_data, frame->nb_samples);
    }

    int frameSize = aState.codecCtx->frame_size > 0 ? aState.codecCtx->frame_size : 1024;

    while (av_audio_fifo_size(aState.fifo) >= frameSize) {
        FramePtr encFrame(av_frame_alloc());
        encFrame->nb_samples = frameSize;
        encFrame->format = aState.codecCtx->sample_fmt;
        Core::Compat::setFrameAudioLayout(encFrame.get(), aState.codecCtx);
        encFrame->sample_rate = aState.codecCtx->sample_rate;
        encFrame->pts = aState.nextPts;
        aState.nextPts += frameSize;

        if (av_frame_get_buffer(encFrame.get(), 0) < 0) break;

        if (av_audio_fifo_read(aState.fifo, (void**)encFrame->extended_data, frameSize) < frameSize) {
            break;
        }

        int ret = avcodec_send_frame(aState.codecCtx, encFrame.get());
        if (ret < 0) break;

        while (ret >= 0) {
            PacketPtr pkt(av_packet_alloc());
            if (!pkt) break;

            ret = avcodec_receive_packet(aState.codecCtx, pkt.get());
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
            if (ret < 0) return false;

            writePacket(pkt.get(), aState.codecCtx->time_base, aState.stream);
        }
    }

    return true;
}

bool MediaEncoder::writeSubtitlePacket(size_t trackIdx, AVPacket* pkt, AVRational srcTimeBase) {
    if (trackIdx >= m_subtitleStreams.size() || !pkt) return false;
    AVStream* subStream = m_subtitleStreams[trackIdx].stream;
    if (!subStream) return false;

    av_packet_rescale_ts(pkt, srcTimeBase, subStream->time_base);
    pkt->stream_index = subStream->index;

    return av_interleaved_write_frame(m_outputFormatCtx, pkt) >= 0;
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
    for (size_t i = 0; i < m_audioStreams.size(); ++i) {
        AudioStreamState& aState = m_audioStreams[i];
        if (aState.fifo && av_audio_fifo_size(aState.fifo) > 0) {
            int remaining = av_audio_fifo_size(aState.fifo);
            FramePtr encFrame(av_frame_alloc());
            encFrame->nb_samples = remaining;
            encFrame->format = aState.codecCtx->sample_fmt;
            Core::Compat::setFrameAudioLayout(encFrame.get(), aState.codecCtx);
            encFrame->sample_rate = aState.codecCtx->sample_rate;
            encFrame->pts = aState.nextPts;
            aState.nextPts += remaining;

            if (av_frame_get_buffer(encFrame.get(), 0) >= 0) {
                av_audio_fifo_read(aState.fifo, (void**)encFrame->extended_data, remaining);
                avcodec_send_frame(aState.codecCtx, encFrame.get());
                while (true) {
                    PacketPtr pkt(av_packet_alloc());
                    if (avcodec_receive_packet(aState.codecCtx, pkt.get()) < 0) break;
                    writePacket(pkt.get(), aState.codecCtx->time_base, aState.stream);
                }
            }
        }
        encodeAudioFrame(i, nullptr);
    }

    if (m_outputFormatCtx && m_headerWritten) {
        av_write_trailer(m_outputFormatCtx);
    }

    if (m_videoCodecCtx) avcodec_free_context(&m_videoCodecCtx);
    for (auto& aState : m_audioStreams) {
        if (aState.fifo) av_audio_fifo_free(aState.fifo);
        if (aState.codecCtx) avcodec_free_context(&aState.codecCtx);
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