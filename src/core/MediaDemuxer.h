#ifndef MEDIA_DEMUXER_H
#define MEDIA_DEMUXER_H

#include "RAIIWrappers.h"
#include <string>
#include <vector>
#include <map>

namespace Core {

struct AudioTrackInfo {
    int index = -1;
    std::string codecName;
    std::string codecLongName;
    int sampleRate = 0;
    int channels = 0;
    int bitRate = 0;
    std::string language;
    std::string title;
};

struct SubtitleTrackInfo {
    int index = -1;
    std::string codecName;
    std::string codecLongName;
    std::string language;
    std::string title;
    bool isDefault = false;
    bool isForced = false;
};

struct VideoTrackInfo {
    int index = -1;
    std::string codecName;
    std::string codecLongName;
    int width = 0;
    int height = 0;
    double fps = 0.0;
    std::string pixelFormat;
    int bitRate = 0;
    std::string language;
    std::string title;
};

struct MediaInfo {
    std::string filePath;
    std::string formatName;
    std::string formatLongName;
    double durationSeconds = 0.0;
    int64_t bitRate = 0;
    
    int videoStreamIndex = -1;
    int audioStreamIndex = -1;
    int width = 0;
    int height = 0;
    std::string videoCodecName;
    std::string audioCodecName;

    std::vector<VideoTrackInfo> videoTracks;
    std::vector<AudioTrackInfo> audioTracks;
    std::vector<SubtitleTrackInfo> subtitleTracks;
    std::map<std::string, std::string> metadataTags;
};

class MediaDemuxer {
public:
    MediaDemuxer() = default;
    ~MediaDemuxer() = default;
    
    AVStream* getVideoStream() const;
    AVStream* getAudioStream() const;
    AVFormatContext* getFormatContext() const { return m_formatContext.get(); }

    // Prevent copying to maintain unique ownership
    MediaDemuxer(const MediaDemuxer&) = delete;
    MediaDemuxer& operator=(const MediaDemuxer&) = delete;

    bool openFile(const std::string& filePath);
    const MediaInfo& getInfo() const { return m_info; }

private:
    FormatContextPtr m_formatContext;
    MediaInfo m_info;
};

} // namespace Core

#endif // MEDIA_DEMUXER_H