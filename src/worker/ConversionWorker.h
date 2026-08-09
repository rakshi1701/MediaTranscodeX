#ifndef CONVERSION_WORKER_H
#define CONVERSION_WORKER_H

#include <QObject>
#include <QString>
#include <atomic>
#include "MediaDemuxer.h"
#include "MediaDecoder.h"
#include "MediaEncoder.h"

namespace Worker {

struct ConversionJob {
    QString inputPath;
    QString outputPath;
    int targetWidth = 1280;
    int targetHeight = 720;
    int bitRate = 2000000;
};

class ConversionWorker : public QObject {
    Q_OBJECT

public:
    explicit ConversionWorker(const ConversionJob& job, QObject *parent = nullptr);
    ~ConversionWorker() override = default;

public slots:
    void process();
    void cancel();

signals:
    void progressUpdated(int percentage);
    void statusMessage(const QString& message);
    void conversionFinished(bool success, const QString& message);

private:
    ConversionJob m_job;
    std::atomic<bool> m_cancelRequested{false};
};

} // namespace Worker

#endif // CONVERSION_WORKER_H