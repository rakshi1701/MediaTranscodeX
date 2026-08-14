#include "MediaDemuxer.h"
#include <iostream>

extern "C" {
    #include <libavutil/pixdesc.h>
}

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

    // 3. Container format info and metadata tags
    if (m_formatContext->iformat) {
        if (m_formatContext->iformat->name) m_info.formatName = m_formatContext->iformat->name;
        if (m_formatContext->iformat->long_name) m_info.formatLongName = m_formatContext->iformat->long_name;
    }
    m_info.bitRate = m_formatContext->bit_rate;

    if (m_formatContext->duration != AV_NOPTS_VALUE) {
        m_info.durationSeconds = static_cast<double>(m_formatContext->duration) / AV_TIME_BASE;
    }

    // Extract Container Metadata Tags (Description, Title, Artist, Creation time, etc.)
    AVDictionaryEntry *tag = nullptr;
    while ((tag = av_dict_get(m_formatContext->metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {
        if (tag->key && tag->value) {
            m_info.metadataTags[tag->key] = tag->value;
        }
    }

    // 4. Locate best video and audio stream indices
    m_info.videoStreamIndex = av_find_best_stream(m_formatContext.get(), AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    m_info.audioStreamIndex = av_find_best_stream(m_formatContext.get(), AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);

    // 5. Iterate over ALL streams and collect detailed stream info
    for (unsigned int i = 0; i < m_formatContext->nb_streams; ++i) {
        AVStream* st = m_formatContext->streams[i];
        if (!st || !st->codecpar) continue;

        std::string lang = "";
        std::string title = "";
        AVDictionaryEntry* langTag = av_dict_get(st->metadata, "language", nullptr, 0);
        if (langTag && langTag->value) lang = langTag->value;
        AVDictionaryEntry* titleTag = av_dict_get(st->metadata, "title", nullptr, 0);
        if (titleTag && titleTag->value) title = titleTag->value;

        const AVCodec* codec = avcodec_find_decoder(st->codecpar->codec_id);
        std::string codecName = codec ? codec->name : "unknown";
        std::string codecLongName = codec && codec->long_name ? codec->long_name : codecName;

        if (st->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            VideoTrackInfo vInfo;
            vInfo.index = i;
            vInfo.codecName = codecName;
            vInfo.codecLongName = codecLongName;
            vInfo.width = st->codecpar->width;
            vInfo.height = st->codecpar->height;
            vInfo.bitRate = static_cast<int>(st->codecpar->bit_rate);
            vInfo.language = lang;
            vInfo.title = title;
            if (st->avg_frame_rate.den > 0) {
                vInfo.fps = av_q2d(st->avg_frame_rate);
            }
            const char* pixFmtName = av_get_pix_fmt_name(static_cast<AVPixelFormat>(st->codecpar->format));
            vInfo.pixelFormat = pixFmtName ? pixFmtName : "unknown";
            
            m_info.videoTracks.push_back(vInfo);

            if (static_cast<int>(i) == m_info.videoStreamIndex) {
                m_info.width = vInfo.width;
                m_info.height = vInfo.height;
                m_info.videoCodecName = codecName;
            }
        }
        else if (st->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            AudioTrackInfo aInfo;
            aInfo.index = i;
            aInfo.codecName = codecName;
            aInfo.codecLongName = codecLongName;
            aInfo.sampleRate = st->codecpar->sample_rate;
            aInfo.channels = Core::Compat::getChannels(st->codecpar);
            aInfo.bitRate = static_cast<int>(st->codecpar->bit_rate);
            aInfo.language = lang;
            aInfo.title = title;

            m_info.audioTracks.push_back(aInfo);

            if (static_cast<int>(i) == m_info.audioStreamIndex) {
                m_info.audioCodecName = codecName;
            }
        }
        else if (st->codecpar->codec_type == AVMEDIA_TYPE_SUBTITLE) {
            SubtitleTrackInfo sInfo;
            sInfo.index = i;
            sInfo.codecName = codecName;
            sInfo.codecLongName = codecLongName;
            sInfo.language = lang;
            sInfo.title = title;
            sInfo.isDefault = (st->disposition & AV_DISPOSITION_DEFAULT) != 0;
            sInfo.isForced = (st->disposition & AV_DISPOSITION_FORCED) != 0;

            m_info.subtitleTracks.push_back(sInfo);
        }
    }

    return true;
}

} // namespace Core