#include "ConversionWorker.h"
#include "MediaDemuxer.h"
#include "MediaDecoder.h"
#include "MediaEncoder.h"
#include "AudioResampler.h"
#include "FrameScaler.h"
#include <QDebug>
#include <vector>
#include <memory>

namespace Worker {

struct AudioPipeline {
    size_t outputTrackIndex = 0;
    Core::TrackSourceType sourceType = Core::TrackSourceType::Internal;
    int streamIndex = -1;
    std::shared_ptr<Core::MediaDemuxer> externalDemuxer;
    std::unique_ptr<Core::MediaDecoder> decoder;
    std::unique_ptr<Core::AudioResampler> resampler;
    double startTimeSec = 0.0;
    double maxDurationSec = 0.0;
};

struct SubtitlePipeline {
    size_t outputTrackIndex = 0;
    Core::TrackSourceType sourceType = Core::TrackSourceType::Internal;
    int streamIndex = -1;
    std::shared_ptr<Core::MediaDemuxer> externalDemuxer;
    AVRational timeBase = AVRational{1, 1000};
};

ConversionWorker::ConversionWorker(const Core::TranscodeOptions& options, QObject *parent)
    : QObject(parent), m_options(options) {}

void ConversionWorker::cancel() {
    m_cancelRequested = true;
}

void ConversionWorker::process() {
    emit statusMessage("Opening primary input file...");

    auto primaryDemuxer = std::make_shared<Core::MediaDemuxer>();
    if (!primaryDemuxer->openFile(m_options.inputFilePath)) {
        emit conversionFinished(false, "Failed to open primary media file.");
        return;
    }

    AVStream* videoStream = primaryDemuxer->getVideoStream();
    bool hasVideo = (videoStream != nullptr) && m_options.video.enableVideo;

    // Filter enabled Audio and Subtitle tracks
    std::vector<Core::TrackSelection> enabledAudioSelections;
    for (const auto& a : m_options.selectedAudioTracks) {
        if (a.enabled) enabledAudioSelections.push_back(a);
    }
    // If no explicit selections were provided, default to best internal audio if available
    if (m_options.selectedAudioTracks.empty() && primaryDemuxer->getAudioStream() && m_options.audio.enableAudio) {
        Core::TrackSelection defaultAudio;
        defaultAudio.type = Core::TrackType::Audio;
        defaultAudio.sourceType = Core::TrackSourceType::Internal;
        defaultAudio.sourceStreamIndex = primaryDemuxer->getInfo().audioStreamIndex;
        defaultAudio.enabled = true;
        enabledAudioSelections.push_back(defaultAudio);
    }

    std::vector<Core::TrackSelection> enabledSubtitleSelections;
    for (const auto& s : m_options.selectedSubtitleTracks) {
        if (s.enabled) enabledSubtitleSelections.push_back(s);
    }

    // Initialize Video Decoder
    Core::MediaDecoder videoDecoder;
    if (hasVideo && !videoDecoder.init(videoStream)) {
        emit conversionFinished(false, "Failed to initialize video decoder.");
        return;
    }

    int srcW = 0, srcH = 0;
    AVPixelFormat srcFmt = AV_PIX_FMT_NONE;
    if (hasVideo) {
        srcW = videoDecoder.getCodecContext()->width;
        srcH = videoDecoder.getCodecContext()->height;
        srcFmt = videoDecoder.getCodecContext()->pix_fmt;
        if (m_options.video.targetWidth <= 0)  m_options.video.targetWidth = srcW;
        if (m_options.video.targetHeight <= 0) m_options.video.targetHeight = srcH;
    }

    bool needsScaling = hasVideo && (m_options.video.targetWidth != srcW ||
                                     m_options.video.targetHeight != srcH ||
                                     m_options.video.pixFmt != srcFmt);

    Core::FrameScaler videoScaler;
    if (needsScaling) {
        if (!videoScaler.init(srcW, srcH, srcFmt, m_options.video.targetWidth, m_options.video.targetHeight, m_options.video.pixFmt)) {
            emit conversionFinished(false, "Failed to initialize video frame scaler.");
            return;
        }
    }

    // Initialize Output MediaEncoder
    Core::MediaEncoder encoder;
    if (!encoder.init(m_options, hasVideo, enabledAudioSelections.size(), enabledSubtitleSelections.size())) {
        emit conversionFinished(false, "Failed to initialize output encoder.");
        return;
    }

    // Setup Audio Pipelines
    std::vector<AudioPipeline> audioPipelines;
    for (size_t i = 0; i < enabledAudioSelections.size(); ++i) {
        const auto& sel = enabledAudioSelections[i];
        AudioPipeline pipe;
        pipe.outputTrackIndex = i;
        pipe.sourceType = sel.sourceType;
        pipe.startTimeSec = sel.startTimeSec;
        pipe.maxDurationSec = sel.maxDurationSec;

        AVStream* aStream = nullptr;
        if (sel.sourceType == Core::TrackSourceType::Internal) {
            pipe.streamIndex = sel.sourceStreamIndex;
            if (pipe.streamIndex >= 0 && pipe.streamIndex < static_cast<int>(primaryDemuxer->getFormatContext()->nb_streams)) {
                aStream = primaryDemuxer->getFormatContext()->streams[pipe.streamIndex];
            }
        } else {
            pipe.externalDemuxer = std::make_shared<Core::MediaDemuxer>();
            if (pipe.externalDemuxer->openFile(sel.externalFilePath)) {
                aStream = pipe.externalDemuxer->getAudioStream();
                if (aStream) {
                    pipe.streamIndex = pipe.externalDemuxer->getInfo().audioStreamIndex;
                }
            }
        }

        if (aStream) {
            pipe.decoder = std::make_unique<Core::MediaDecoder>();
            if (pipe.decoder->init(aStream)) {
                AVCodecContext* decCtx = pipe.decoder->getCodecContext();
                AVCodecContext* encCtx = encoder.getAudioCodecContext(i);
                if (encCtx) {
                    uint64_t inLayout = Core::Compat::getChannelLayout(decCtx);
                    uint64_t outLayout = Core::Compat::getChannelLayout(encCtx);
                    
                    pipe.resampler = std::make_unique<Core::AudioResampler>();
                    if (pipe.resampler->init(inLayout, decCtx->sample_fmt, decCtx->sample_rate,
                                            outLayout, encCtx->sample_fmt, encCtx->sample_rate)) {
                        audioPipelines.push_back(std::move(pipe));
                    }
                }
            }
        }
    }

    // Setup Subtitle Pipelines
    std::vector<SubtitlePipeline> subPipelines;
    for (size_t j = 0; j < enabledSubtitleSelections.size(); ++j) {
        const auto& sel = enabledSubtitleSelections[j];
        SubtitlePipeline pipe;
        pipe.outputTrackIndex = j;
        pipe.sourceType = sel.sourceType;

        AVStream* sStream = nullptr;
        if (sel.sourceType == Core::TrackSourceType::Internal) {
            pipe.streamIndex = sel.sourceStreamIndex;
            if (pipe.streamIndex >= 0 && pipe.streamIndex < static_cast<int>(primaryDemuxer->getFormatContext()->nb_streams)) {
                sStream = primaryDemuxer->getFormatContext()->streams[pipe.streamIndex];
            }
        } else {
            pipe.externalDemuxer = std::make_shared<Core::MediaDemuxer>();
            if (pipe.externalDemuxer->openFile(sel.externalFilePath)) {
                if (pipe.externalDemuxer->getFormatContext()->nb_streams > 0) {
                    sStream = pipe.externalDemuxer->getFormatContext()->streams[0];
                    pipe.streamIndex = 0;
                }
            }
        }

        if (sStream) {
            pipe.timeBase = sStream->time_base;
            subPipelines.push_back(std::move(pipe));
        }
    }

    emit statusMessage("Transcoding primary video and active audio/subtitle tracks...");

    double totalDuration = primaryDemuxer->getInfo().durationSeconds;
    int vIdx = primaryDemuxer->getInfo().videoStreamIndex;

    Core::PacketPtr packet(av_packet_alloc());

    // 1. Demux & Process Primary Container
    while (av_read_frame(primaryDemuxer->getFormatContext(), packet.get()) >= 0) {
        if (m_cancelRequested) {
            emit statusMessage("Cancelled by user.");
            emit conversionFinished(false, "Transcoding cancelled.");
            return;
        }

        // Primary Video
        if (hasVideo && packet->stream_index == vIdx) {
            if (totalDuration > 0 && packet->pts != AV_NOPTS_VALUE) {
                double sec = packet->pts * av_q2d(videoStream->time_base);
                int progress = static_cast<int>((sec / totalDuration) * 100.0);
                emit progressUpdated(std::min(100, std::max(0, progress)));
            }

            videoDecoder.decodePacket(packet.get(), [&](AVFrame* frame) {
                if (needsScaling) {
                    Core::FramePtr scaledFrame = videoScaler.scaleFrame(frame);
                    if (scaledFrame) {
                        encoder.encodeVideoFrame(scaledFrame.get());
                    }
                } else {
                    encoder.encodeVideoFrame(frame);
                }
            });
        }
        // Primary Internal Audio Streams
        for (auto& aPipe : audioPipelines) {
            if (aPipe.sourceType == Core::TrackSourceType::Internal && packet->stream_index == aPipe.streamIndex) {
                size_t outIdx = aPipe.outputTrackIndex;
                aPipe.decoder->decodePacket(packet.get(), [&](AVFrame* frame) {
                    Core::FramePtr resampledFrame = aPipe.resampler->resampleFrame(frame);
                    if (resampledFrame) {
                        encoder.encodeAudioFrame(outIdx, resampledFrame.get());
                    }
                });
            }
        }
        // Primary Internal Subtitle Streams
        for (auto& sPipe : subPipelines) {
            if (sPipe.sourceType == Core::TrackSourceType::Internal && packet->stream_index == sPipe.streamIndex) {
                encoder.writeSubtitlePacket(sPipe.outputTrackIndex, packet.get(), sPipe.timeBase);
            }
        }

        av_packet_unref(packet.get());
    }

    // 2. Demux & Process External Audio Tracks
    for (auto& aPipe : audioPipelines) {
        if (m_cancelRequested) break;
        if (aPipe.sourceType == Core::TrackSourceType::External && aPipe.externalDemuxer) {
            emit statusMessage("Processing external audio track...");

            // Perform seek if start offset specified
            if (aPipe.startTimeSec > 0.0) {
                int64_t seekTarget = static_cast<int64_t>(aPipe.startTimeSec * AV_TIME_BASE);
                av_seek_frame(aPipe.externalDemuxer->getFormatContext(), -1, seekTarget, AVSEEK_FLAG_BACKWARD);
            }

            double maxAllowedDuration = (aPipe.maxDurationSec > 0.0) ? aPipe.maxDurationSec : totalDuration;
            double processedAudioSec = 0.0;
            bool durationReached = false;

            Core::PacketPtr extPkt(av_packet_alloc());
            size_t outIdx = aPipe.outputTrackIndex;
            while (!durationReached && !m_cancelRequested && av_read_frame(aPipe.externalDemuxer->getFormatContext(), extPkt.get()) >= 0) {
                if (extPkt->stream_index == aPipe.streamIndex) {
                    aPipe.decoder->decodePacket(extPkt.get(), [&](AVFrame* frame) {
                        if (durationReached) return;
                        Core::FramePtr resampledFrame = aPipe.resampler->resampleFrame(frame);
                        if (resampledFrame) {
                            encoder.encodeAudioFrame(outIdx, resampledFrame.get());
                            processedAudioSec += static_cast<double>(resampledFrame->nb_samples) / resampledFrame->sample_rate;
                            if (maxAllowedDuration > 0.0 && processedAudioSec >= maxAllowedDuration) {
                                durationReached = true;
                            }
                        }
                    });
                }
                av_packet_unref(extPkt.get());
            }
        }
    }

    // 3. Demux & Process External Subtitle Tracks
    for (auto& sPipe : subPipelines) {
        if (m_cancelRequested) break;
        if (sPipe.sourceType == Core::TrackSourceType::External && sPipe.externalDemuxer) {
            emit statusMessage("Multiplexing external subtitle track...");
            Core::PacketPtr extPkt(av_packet_alloc());
            while (!m_cancelRequested && av_read_frame(sPipe.externalDemuxer->getFormatContext(), extPkt.get()) >= 0) {
                if (extPkt->stream_index == sPipe.streamIndex) {
                    encoder.writeSubtitlePacket(sPipe.outputTrackIndex, extPkt.get(), sPipe.timeBase);
                }
                av_packet_unref(extPkt.get());
            }
        }
    }

    if (m_cancelRequested) {
        emit statusMessage("Cancelled by user.");
        emit conversionFinished(false, "Transcoding cancelled.");
        return;
    }

    emit statusMessage("Finalizing output container...");
    encoder.finish();

    emit progressUpdated(100);
    emit conversionFinished(true, "Media Stream Modification Complete!");
}

} // namespace Worker