#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QFileDialog>
#include <QPixmap>
#include <QDebug>

#include "MediaDemuxer.h"
#include "MediaDecoder.h"
#include "FrameScaler.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    Core::MediaDemuxer demuxer;
    Core::MediaDecoder decoder;
    Core::FrameScaler scaler;

    QImage extractedPreviewImage;
    bool frameExtracted = false;

    // 1. Prompt user for video file
    QString filePath = QFileDialog::getOpenFileName(
        nullptr,
        "Select Test Video File for Frame Preview",
        "",
        "Video Files (*.mp4 *.mkv *.avi *.webm);;All Files (*)"
    );

    if (!filePath.isEmpty()) {
        std::string samplePath = filePath.toStdString();

        if (demuxer.openFile(samplePath)) {
            AVStream* videoStream = demuxer.getVideoStream();

            if (videoStream && decoder.init(videoStream)) {
                AVCodecContext* codecCtx = decoder.getCodecContext();
                int videoStreamIdx = demuxer.getInfo().videoStreamIndex;

                // Configure scaler to convert raw frames into preview-sized RGB image (640x360)
                int previewWidth = 640;
                int previewHeight = 360;
                scaler.init(codecCtx->width, codecCtx->height, codecCtx->pix_fmt,
                            previewWidth, previewHeight, AV_PIX_FMT_RGB24);

                Core::PacketPtr packet(av_packet_alloc());

                // Read packets until we decode the first video frame
                while (av_read_frame(demuxer.getFormatContext(), packet.get()) >= 0 && !frameExtracted) {
                    if (packet->stream_index == videoStreamIdx) {
                        decoder.decodePacket(packet.get(), [&](AVFrame* frame) {
                            if (!frameExtracted) {
                                // Scale YUV frame to RGB QImage
                                extractedPreviewImage = scaler.scaleToQImage(frame);
                                frameExtracted = !extractedPreviewImage.isNull();
                            }
                        });
                    }
                    av_packet_unref(packet.get());
                }
            }
        }
    }

    // 2. Render GUI with live video preview
    QWidget window;
    window.setWindowTitle("Media_TranscodeX - Sprint 4 Frame Scaler & Preview Test");
    window.resize(680, 480);

    QVBoxLayout *layout = new QVBoxLayout(&window);

    QLabel *infoLabel = new QLabel(&window);
    infoLabel->setAlignment(Qt::AlignCenter);

    QLabel *imageLabel = new QLabel(&window);
    imageLabel->setAlignment(Qt::AlignCenter);

    if (frameExtracted) {
        infoLabel->setText(QString("Successfully decoded and scaled video frame!\nResolution: %1x%2 -> Scaled to 640x360 RGB24")
                           .arg(demuxer.getInfo().width)
                           .arg(demuxer.getInfo().height));

        imageLabel->setPixmap(QPixmap::fromImage(extractedPreviewImage));
    } else {
        infoLabel->setText("No frame extracted or invalid video file selected.");
    }

    layout->addWidget(infoLabel);
    layout->addWidget(imageLabel);

    window.show();
    return app.exec();
}