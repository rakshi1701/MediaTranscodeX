#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QFileDialog>
#include <QDebug>

#include "MediaDemuxer.h"
#include "MediaDecoder.h"
#include "MediaEncoder.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    Core::MediaDemuxer demuxer;
    Core::MediaDecoder decoder;
    Core::MediaEncoder encoder;

    int encodedFrameCount = 0;
    bool success = false;
    std::string outputPath = "/tmp/output_test.mp4";

    // 1. Prompt user for source file
    QString filePath = QFileDialog::getOpenFileName(
        nullptr,
        "Select Source Video File to Transcode",
        "",
        "Video Files (*.mp4 *.mkv *.avi *.webm);;All Files (*)"
    );

    if (!filePath.isEmpty()) {
        std::string samplePath = filePath.toStdString();

        if (demuxer.openFile(samplePath)) {
            AVStream* videoStream = demuxer.getVideoStream();

            if (videoStream && decoder.init(videoStream)) {
                AVCodecContext* decoderCtx = decoder.getCodecContext();

                // Setup output encoder configuration
                Core::EncoderConfig config;
                config.outputFilePath = outputPath;
                config.width = decoderCtx->width;
                config.height = decoderCtx->height;
                config.pixFmt = AV_PIX_FMT_YUV420P; // Standard compatible pixel format
                config.framerate = 30;

                if (encoder.init(config)) {
                    Core::PacketPtr packet(av_packet_alloc());
                    int videoStreamIdx = demuxer.getInfo().videoStreamIndex;

                    // Decode & Re-encode up to 150 frames
                    while (av_read_frame(demuxer.getFormatContext(), packet.get()) >= 0 && encodedFrameCount < 150) {
                        if (packet->stream_index == videoStreamIdx) {
                            decoder.decodePacket(packet.get(), [&](AVFrame* frame) {
                                if (encodedFrameCount < 150) {
                                    encoder.encodeVideoFrame(frame);
                                    encodedFrameCount++;
                                }
                            });
                        }
                        av_packet_unref(packet.get());
                    }

                    // Flush encoder & finalize output container
                    encoder.finish();
                    success = (encodedFrameCount > 0);
                }
            }
        }
    }

    // 2. Render GUI result
    QWidget window;
    window.setWindowTitle("Media_TranscodeX - Sprint 5 Encoder Pipeline Test");
    window.resize(550, 300);

    QVBoxLayout *layout = new QVBoxLayout(&window);

    QString statusText;
    if (success) {
        statusText = QString("Transcoding Test Successful!\n\n"
                             "Source: %1\n"
                             "Target Output: %2\n"
                             "Encoded Video Frames: %3\n"
                             "Status: Transcoded video written and closed successfully!")
                         .arg(filePath)
                         .arg(QString::fromStdString(outputPath))
                         .arg(encodedFrameCount);
    } else {
        statusText = QString("Transcoding failed or no input file selected.");
    }

    QLabel *label = new QLabel(statusText, &window);
    label->setAlignment(Qt::AlignCenter);
    layout->addWidget(label);

    window.show();
    return app.exec();
}