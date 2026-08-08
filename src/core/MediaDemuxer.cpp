#include "MediaDemuxer.h"
#include <iostream>

namespace Core {

    
AVStream* MediaDemuxer::getVideoStream() const {
    if (m_formatContext && m_info.videoStreamIndex >= 0) {
        return m_formatContext->streams[m_info.videoStreamIndex];
    }
    return nullptr;
}

AVStream* MediaDemuxer::getAudioStream() const {
    if (m_formatContext && m_info.audioStreamIndex >= 0) {
        return m_formatContext->streams[m_info.audioStreamIndex];
    }
    return nullptr;
}

bool MediaDemuxer::openFile(const std::string& filePath) {
    m_info = MediaInfo();
    m_info.filePath = filePath;

    AVFormatContext* rawCtx = nullptr;

    // 1. Open input stream and read header
    if (avformat_open_input(&rawCtx, filePath.c_str(), nullptr, nullptr) < 0) {
        std::cerr << "[MediaDemuxer] Error: Could not open file: " << filePath << std::endl;
        return false;
    }

    // Immediately wrap raw pointer into RAII smart pointer
    m_formatContext.reset(rawCtx);

    // 2. Retrieve stream information
    if (avformat_find_stream_info(m_formatContext.get(), nullptr) < 0) {
        std::cerr << "[MediaDemuxer] Error: Could not find stream information." << std::endl;
        return false;
    }

    // 3. Calculate duration in seconds
    if (m_formatContext->duration != AV_NOPTS_VALUE) {
        m_info.durationSeconds = static_cast<double>(m_formatContext->duration) / AV_TIME_BASE;
    }

    // 4. Locate best video and audio stream indices
    m_info.videoStreamIndex = av_find_best_stream(m_formatContext.get(), AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    m_info.audioStreamIndex = av_find_best_stream(m_formatContext.get(), AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);

    // Extract Video details
    if (m_info.videoStreamIndex >= 0) {
        AVStream* videoStream = m_formatContext->streams[m_info.videoStreamIndex];
        m_info.width = videoStream->codecpar->width;
        m_info.height = videoStream->codecpar->height;
        const AVCodec* codec = avcodec_find_decoder(videoStream->codecpar->codec_id);
        if (codec) {
            m_info.videoCodecName = codec->name;
        }
    }

    // Extract Audio details
    if (m_info.audioStreamIndex >= 0) {
        AVStream* audioStream = m_formatContext->streams[m_info.audioStreamIndex];
        const AVCodec* codec = avcodec_find_decoder(audioStream->codecpar->codec_id);
        if (codec) {
            m_info.audioCodecName = codec->name;
        }
    }

    return true;
}

} // namespace Core