#include "ConversionWorker.h"
#include <QDebug>

namespace Worker {

ConversionWorker::ConversionWorker(const ConversionJob& job, QObject *parent)
    : QObject(parent), m_job(job) {}

void ConversionWorker::cancel() {
    m_cancelRequested = true;
}

void ConversionWorker::process() {
    emit statusMessage("Opening input media file...");

    Core::MediaDemuxer demuxer;
    if (!demuxer.openFile(m_job.inputPath.toStdString())) {
        emit conversionFinished(false, "Failed to open input file.");
        return;
    }

    AVStream* videoStream = demuxer.getVideoStream();
    if (!videoStream) {
        emit conversionFinished(false, "No valid video stream found.");
        return;
    }

    Core::MediaDecoder decoder;
    if (!decoder.init(videoStream)) {
        emit conversionFinished(false, "Failed to initialize decoder.");
        return;
    }

    AVCodecContext* decoderCtx = decoder.getCodecContext();

    Core::EncoderConfig config;
    config.outputFilePath = m_job.outputPath.toStdString();
    config.width = (m_job.targetWidth > 0) ? m_job.targetWidth : decoderCtx->width;
    config.height = (m_job.targetHeight > 0) ? m_job.targetHeight : decoderCtx->height;
    config.bitRate = m_job.bitRate;

    Core::MediaEncoder encoder;
    if (!encoder.init(config)) {
        emit conversionFinished(false, "Failed to initialize output encoder.");
        return;
    }

    emit statusMessage("Transcoding in progress...");

    double totalDuration = demuxer.getInfo().durationSeconds;
    int videoStreamIdx = demuxer.getInfo().videoStreamIndex;
    int64_t processedFrames = 0;

    Core::PacketPtr packet(av_packet_alloc());
    AVRational timeBase = videoStream->time_base;

    while (av_read_frame(demuxer.getFormatContext(), packet.get()) >= 0) {
        if (m_cancelRequested) {
            emit statusMessage("Conversion cancelled by user.");
            emit conversionFinished(false, "Cancelled.");
            return;
        }

        if (packet->stream_index == videoStreamIdx) {
            // Calculate progress based on frame timestamps
            if (totalDuration > 0 && packet->pts != AV_NOPTS_VALUE) {
                double currentSeconds = packet->pts * av_q2d(timeBase);
                int progress = static_cast<int>((currentSeconds / totalDuration) * 100.0);
                progress = std::min(100, std::max(0, progress));
                emit progressUpdated(progress);
            }

            decoder.decodePacket(packet.get(), [&](AVFrame* frame) {
                encoder.encodeVideoFrame(frame);
                processedFrames++;
            });
        }
        av_packet_unref(packet.get());
    }

    emit statusMessage("Finalizing output container...");
    encoder.finish();

    emit progressUpdated(100);
    emit conversionFinished(true, QString("Transcoding complete! Total frames processed: %1").arg(processedFrames));
}

} // namespace Worker