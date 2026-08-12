#include "ConversionWorker.h"
#include "MediaDemuxer.h"
#include "MediaDecoder.h"
#include "MediaEncoder.h"
#include "AudioResampler.h"
#include <QDebug>

namespace Worker {

ConversionWorker::ConversionWorker(const Core::TranscodeOptions& options, QObject *parent)
    : QObject(parent), m_options(options) {}

void ConversionWorker::cancel() {
    m_cancelRequested = true;
}

void ConversionWorker::process() {
    emit statusMessage("Opening input file...");

    Core::MediaDemuxer demuxer;
    if (!demuxer.openFile(m_options.inputFilePath)) {
        emit conversionFinished(false, "Failed to open input media file.");
        return;
    }

    AVStream* videoStream = demuxer.getVideoStream();
    AVStream* audioStream = demuxer.getAudioStream();

    bool hasVideo = (videoStream != nullptr) && m_options.video.enableVideo;
    bool hasAudio = (audioStream != nullptr) && m_options.audio.enableAudio;

    // Initialize Decoders
    Core::MediaDecoder videoDecoder;
    Core::MediaDecoder audioDecoder;

    if (hasVideo && !videoDecoder.init(videoStream)) {
        emit conversionFinished(false, "Failed to initialize video decoder.");
        return;
    }

    if (hasAudio && !audioDecoder.init(audioStream)) {
        emit conversionFinished(false, "Failed to initialize audio decoder.");
        return;
    }

    // Auto-fill video options from input if unset
    if (hasVideo) {
        if (m_options.video.targetWidth <= 0)  m_options.video.targetWidth = videoDecoder.getCodecContext()->width;
        if (m_options.video.targetHeight <= 0) m_options.video.targetHeight = videoDecoder.getCodecContext()->height;
    }

    // Initialize Encoder
    Core::MediaEncoder encoder;
    if (!encoder.init(m_options, hasVideo, hasAudio)) {
        emit conversionFinished(false, "Failed to initialize output encoder.");
        return;
    }

    // Initialize Audio Resampler if needed
    Core::AudioResampler resampler;
    if (hasAudio) {
        AVCodecContext* audioDecCtx = audioDecoder.getCodecContext();
        AVCodecContext* audioEncCtx = encoder.getAudioCodecContext();

        uint64_t inLayout = audioDecCtx->channel_layout ? audioDecCtx->channel_layout : av_get_default_channel_layout(audioDecCtx->channels);
        uint64_t outLayout = audioEncCtx->channel_layout ? audioEncCtx->channel_layout : av_get_default_channel_layout(audioEncCtx->channels);

        if (!resampler.init(
                inLayout, audioDecCtx->sample_fmt, audioDecCtx->sample_rate,
                outLayout, audioEncCtx->sample_fmt, audioEncCtx->sample_rate)) {
            emit conversionFinished(false, "Failed to initialize audio resampler.");
            return;
        }
    }

    emit statusMessage("Transcoding video and audio streams...");

    double totalDuration = demuxer.getInfo().durationSeconds;
    int vIdx = demuxer.getInfo().videoStreamIndex;
    int aIdx = demuxer.getInfo().audioStreamIndex;

    Core::PacketPtr packet(av_packet_alloc());

    while (av_read_frame(demuxer.getFormatContext(), packet.get()) >= 0) {
        if (m_cancelRequested) {
            emit statusMessage("Cancelled by user.");
            emit conversionFinished(false, "Transcoding cancelled.");
            return;
        }

        // Handle Video Packets
        if (hasVideo && packet->stream_index == vIdx) {
            if (totalDuration > 0 && packet->pts != AV_NOPTS_VALUE) {
                double sec = packet->pts * av_q2d(videoStream->time_base);
                int progress = static_cast<int>((sec / totalDuration) * 100.0);
                emit progressUpdated(std::min(100, std::max(0, progress)));
            }

            videoDecoder.decodePacket(packet.get(), [&](AVFrame* frame) {
                encoder.encodeVideoFrame(frame);
            });
        }
        // Handle Audio Packets
        else if (hasAudio && packet->stream_index == aIdx) {
            audioDecoder.decodePacket(packet.get(), [&](AVFrame* frame) {
                Core::FramePtr resampledFrame = resampler.resampleFrame(frame);
                if (resampledFrame) {
                    encoder.encodeAudioFrame(resampledFrame.get());
                }
            });
        }

        av_packet_unref(packet.get());
    }

    emit statusMessage("Finalizing output container...");
    encoder.finish();

    emit progressUpdated(100);
    emit conversionFinished(true, "Phase 2.1 Complete: Video and Audio Transcoded & Resampled!");
}

} // namespace Worker