#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QFileDialog>
#include <QDebug>
#include "MediaDemuxer.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    Core::MediaDemuxer demuxer;
    bool success = false;

    // 1. Open Native File Dialog
    QString filePath = QFileDialog::getOpenFileName(
        nullptr,
        "Select Test Media File",
        "",
        "Media Files (*.mp4 *.mkv *.avi *.mp3 *.wav *.webm);;All Files (*)"
    );

    // 2. Only attempt to open if the user actually picked a file
    if (!filePath.isEmpty()) {
        std::string samplePath = filePath.toStdString();
        success = demuxer.openFile(samplePath);
    }

    // 3. Render GUI output
    QWidget window;
    window.setWindowTitle("Media_TranscodeX - Sprint 2 Demuxer Test");
    window.resize(550, 300);

    QVBoxLayout *layout = new QVBoxLayout(&window);

    QString statusText;
    if (success) {
        const auto& info = demuxer.getInfo();
        statusText = QString("File Loaded Successfully!\n\n"
                             "Path: %1\n"
                             "Duration: %2 sec\n"
                             "Resolution: %3x%4\n"
                             "Video Codec: %5\n"
                             "Audio Codec: %6")
                         .arg(QString::fromStdString(info.filePath))
                         .arg(info.durationSeconds)
                         .arg(info.width)
                         .arg(info.height)
                         .arg(QString::fromStdString(info.videoCodecName))
                         .arg(QString::fromStdString(info.audioCodecName));
    } else {
        statusText = QString("No file selected or failed to parse media file.");
    }

    QLabel *label = new QLabel(statusText, &window);
    label->setAlignment(Qt::AlignCenter);
    layout->addWidget(label);

    window.show();
    return app.exec();
}