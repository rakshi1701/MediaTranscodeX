#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <QMainWindow>
#include <QLineEdit>
#include <QPushButton>
#include <QProgressBar>
#include <QLabel>
#include <QComboBox>
#include <QThread>
#include <QTreeWidget>
#include <QHeaderView>

#include "ConversionWorker.h"
#include "MediaDemuxer.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void browseInputFile();
    void browseOutputFile();
    void startConversion();
    void cancelConversion();
    void inspectFile(const QString& filePath);

    void onProgressUpdated(int percentage);
    void onStatusMessage(const QString& message);
    void onConversionFinished(bool success, const QString& message);

private:
    void setupUi();

    QLineEdit *m_inputPathEdit = nullptr;
    QLineEdit *m_outputPathEdit = nullptr;
    QComboBox *m_presetCombo = nullptr;
    QProgressBar *m_progressBar = nullptr;
    QLabel *m_statusLabel = nullptr;
    QPushButton *m_startBtn = nullptr;
    QPushButton *m_cancelBtn = nullptr;
    QTreeWidget *m_infoTree = nullptr;

    QThread *m_workerThread = nullptr;
    Worker::ConversionWorker *m_worker = nullptr;
};

#endif // MAIN_WINDOW_H