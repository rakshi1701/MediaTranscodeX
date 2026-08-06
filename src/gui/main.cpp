#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QDebug>

// FFmpeg is a C library, so we must wrap its includes in 'extern "C"'
extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libavutil/avutil.h>
}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    // 1. Grab the FFmpeg version to prove linking works
    const char* ffmpegVersion = av_version_info();
    qDebug() << "Successfully linked FFmpeg version:" << ffmpegVersion;

    // 2. Create a basic Qt Window
    QWidget window;
    window.setWindowTitle("FFmpeg + Qt C++ Converter - Sprint 1");
    window.resize(400, 200);

    QVBoxLayout *layout = new QVBoxLayout(&window);
    
    QString labelText = QString("Welcome to Sprint 1!\nFFmpeg Version: %1").arg(ffmpegVersion);
    QLabel *label = new QLabel(labelText, &window);
    label->setAlignment(Qt::AlignCenter);
    
    layout->addWidget(label);
    window.show();

    return app.exec();
}