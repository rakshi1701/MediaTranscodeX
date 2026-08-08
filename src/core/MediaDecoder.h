#ifndef MEDIA_DECODER_H
#define MEDIA_DECODER_H

#include "RAIIWrappers.h"
#include <functional>

namespace Core {

class MediaDecoder {
public:
    MediaDecoder() = default;
    ~MediaDecoder() = default;

    MediaDecoder(const MediaDecoder&) = delete;
    MediaDecoder& operator=(const MediaDecoder&) = delete;

    // Initializes decoder for a given stream (video or audio)
    bool init(AVStream* stream);

    // Sends packet to decoder and pulls all available raw AVFrames
    bool decodePacket(AVPacket* packet, const std::function<void(AVFrame*)>& frameHandler);

    // Flushes buffered frames at end-of-file
    bool flush(const std::function<void(AVFrame*)>& frameHandler);

    AVCodecContext* getCodecContext() const { return m_codecContext.get(); }

private:
    CodecContextPtr m_codecContext;
    int m_streamIndex = -1;
};

} // namespace Core

#endif // MEDIA_DECODER_H