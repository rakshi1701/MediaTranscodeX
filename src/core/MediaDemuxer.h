#ifndef MEDIA_DEMUXER_H
#define MEDIA_DEMUXER_H

#include "RAIIWrappers.h"
#include <string>

namespace Core {

struct MediaInfo {
    std::string filePath;
    double durationSeconds = 0.0;
    int videoStreamIndex = -1;
    int audioStreamIndex = -1;
    int width = 0;
    int height = 0;
    std::string videoCodecName;
    std::string audioCodecName;
};

class MediaDemuxer {
public:
    MediaDemuxer() = default;
    ~MediaDemuxer() = default;

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