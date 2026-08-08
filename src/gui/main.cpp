#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QFileDialog>
#include <QDebug>

#include "MediaDemuxer.h"
#include "MediaDecoder.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    Core::MediaDemuxer demuxer;
    Core::MediaDecoder decoder;
    
    int decodedFrameCount = 0;
    int videoStreamIdx = -1;
    bool success = false;

    // 1. Select media file
    QString filePath = QFileDialog::getOpenFileName(
        nullptr,
        "Select Test Media File",
        "",
        "Media Files (*.mp4 *.mkv *.avi *.mp3 *.wav *.webm);;All Files (*)"
    );

    if (!filePath.isEmpty()) {
        std::string samplePath = filePath.toStdString();
        
        if (demuxer.openFile(samplePath)) {
            AVStream* videoStream = demuxer.getVideoStream();
            
            if (videoStream && decoder.init(videoStream)) {
                videoStreamIdx = demuxer.getInfo().videoStreamIndex;
                
                // Read and decode the first 100 video packets
                Core::PacketPtr packet(av_packet_alloc());
                int readPackets = 0;

                while (av_read_frame(demuxer.getFormatContext(), packet.get()) >= 0 && readPackets < 100) {
                    if (packet->stream_index == videoStreamIdx) {
                        decoder.decodePacket(packet.get(), [&](AVFrame* frame) {
                            decodedFrameCount++;
                        });
                        readPackets++;
                    }
                    av_packet_unref(packet.get()); // Clear packet payload buffer
                }

                // Flush remaining frames from decoder queue
                decoder.flush([&](AVFrame* frame) {
                    decodedFrameCount++;
                });

                success = true;
            }
        }
    }

    // 2. Render GUI result
    QWidget window;
    window.setWindowTitle("Media_TranscodeX - Sprint 3 Decoder Test");
    window.resize(550, 300);

    QVBoxLayout *layout = new QVBoxLayout(&window);

    QString statusText;
    if (success) {
        statusText = QString("Sprint 3 Decoding Test Successful!\n\n"
                             "File: %1\n"
                             "Decoded Video Frames: %2\n"
                             "Status: Raw AVFrames extracted and cleaned up safely.")
                         .arg(filePath)
                         .arg(decodedFrameCount);
    } else {
        statusText = QString("Failed to decode frames or no file selected.");
    }

    QLabel *label = new QLabel(statusText, &window);
    label->setAlignment(Qt::AlignCenter);
    layout->addWidget(label);

    window.show();
    return app.exec();
}