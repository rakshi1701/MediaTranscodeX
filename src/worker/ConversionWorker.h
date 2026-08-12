#ifndef CONVERSION_WORKER_H
#define CONVERSION_WORKER_H

#include <QObject>
#include <QString>
#include <atomic>
#include "TranscodeOptions.h"

namespace Worker {

class ConversionWorker : public QObject {
    Q_OBJECT

public:
    explicit ConversionWorker(const Core::TranscodeOptions& options, QObject *parent = nullptr);
    ~ConversionWorker() override = default;

public slots:
    void process();
    void cancel();

signals:
    void progressUpdated(int percentage);
    void statusMessage(const QString& message);
    void conversionFinished(bool success, const QString& message);

private:
    Core::TranscodeOptions m_options;
    std::atomic<bool> m_cancelRequested{false};
};

} // namespace Worker

#endif // CONVERSION_WORKER_H