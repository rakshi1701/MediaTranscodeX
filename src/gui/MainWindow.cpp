#include "MainWindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setupUi();
}

MainWindow::~MainWindow() {
    if (m_workerThread && m_workerThread->isRunning()) {
        m_workerThread->quit();
        m_workerThread->wait();
    }
}

void MainWindow::setupUi() {
    setWindowTitle("Media_TranscodeX - Desktop Media Converter");
    resize(600, 350);

    QWidget *centralWidget = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

    // Input File Row
    QHBoxLayout *inputLayout = new QHBoxLayout();
    m_inputPathEdit = new QLineEdit(this);
    m_inputPathEdit->setPlaceholderText("Select source video/audio file...");
    QPushButton *browseInputBtn = new QPushButton("Browse...", this);
    connect(browseInputBtn, &QPushButton::clicked, this, &MainWindow::browseInputFile);
    inputLayout->addWidget(new QLabel("Source File:", this));
    inputLayout->addWidget(m_inputPathEdit);
    inputLayout->addWidget(browseInputBtn);

    // Output File Row
    QHBoxLayout *outputLayout = new QHBoxLayout();
    m_outputPathEdit = new QLineEdit(this);
    m_outputPathEdit->setPlaceholderText("Target output path...");
    QPushButton *browseOutputBtn = new QPushButton("Save As...", this);
    connect(browseOutputBtn, &QPushButton::clicked, this, &MainWindow::browseOutputFile);
    outputLayout->addWidget(new QLabel("Output File:", this));
    outputLayout->addWidget(m_outputPathEdit);
    outputLayout->addWidget(browseOutputBtn);

    // Preset Options
    QHBoxLayout *presetLayout = new QHBoxLayout();
    m_presetCombo = new QComboBox(this);
    m_presetCombo->addItem("H.264 MP4 - High Quality (1080p, 4Mbps)", 4000000);
    m_presetCombo->addItem("H.264 MP4 - Standard Quality (720p, 2Mbps)", 2000000);
    m_presetCombo->addItem("H.264 MP4 - Low Bitrate (480p, 1Mbps)", 1000000);
    presetLayout->addWidget(new QLabel("Preset:", this));
    presetLayout->addWidget(m_presetCombo);

    // Progress Bar & Status
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);

    m_statusLabel = new QLabel("Ready.", this);
    m_statusLabel->setAlignment(Qt::AlignCenter);

    // Action Buttons
    QHBoxLayout *btnLayout = new QHBoxLayout();
    m_startBtn = new QPushButton("Start Transcoding", this);
    m_cancelBtn = new QPushButton("Cancel", this);
    m_cancelBtn->setEnabled(false);

    connect(m_startBtn, &QPushButton::clicked, this, &MainWindow::startConversion);
    connect(m_cancelBtn, &QPushButton::clicked, this, &MainWindow::cancelConversion);

    btnLayout->addWidget(m_startBtn);
    btnLayout->addWidget(m_cancelBtn);

    // Assemble Layout
    mainLayout->addLayout(inputLayout);
    mainLayout->addLayout(outputLayout);
    mainLayout->addLayout(presetLayout);
    mainLayout->addWidget(m_progressBar);
    mainLayout->addWidget(m_statusLabel);
    mainLayout->addLayout(btnLayout);

    setCentralWidget(centralWidget);
}

void MainWindow::browseInputFile() {
    QString path = QFileDialog::getOpenFileName(this, "Select Source File", "", "Media Files (*.mp4 *.mkv *.avi *.webm *.mp3);;All Files (*)");
    if (!path.isEmpty()) {
        m_inputPathEdit->setText(path);
        QFileInfo info(path);
        m_outputPathEdit->setText(info.absolutePath() + "/" + info.completeBaseName() + "_converted.mp4");
    }
}

void MainWindow::browseOutputFile() {
    QString path = QFileDialog::getSaveFileName(this, "Save Converted File", "", "MP4 Video (*.mp4);;All Files (*)");
    if (!path.isEmpty()) {
        m_outputPathEdit->setText(path);
    }
}

// void MainWindow::startConversion() {
//     if (m_inputPathEdit->text().isEmpty() || m_outputPathEdit->text().isEmpty()) {
//         QMessageBox::warning(this, "Missing Path", "Please select valid input and output file paths.");
//         return;
//     }

//     Worker::ConversionJob job;
//     job.inputPath = m_inputPathEdit->text();
//     job.outputPath = m_outputPathEdit->text();
//     job.bitRate = m_presetCombo->currentData().toInt();

//     m_workerThread = new QThread(this);
//     m_worker = new Worker::ConversionWorker(job);
//     m_worker->moveToThread(m_workerThread);

//     // Thread Wiring
//     connect(m_workerThread, &QThread::started, m_worker, &Worker::ConversionWorker::process);
//     connect(m_worker, &Worker::ConversionWorker::progressUpdated, this, &MainWindow::onProgressUpdated);
//     connect(m_worker, &Worker::ConversionWorker::statusMessage, this, &MainWindow::onStatusMessage);
//     connect(m_worker, &Worker::ConversionWorker::conversionFinished, this, &MainWindow::onConversionFinished);

//     // Cleanup wiring
//     connect(m_worker, &Worker::ConversionWorker::conversionFinished, m_workerThread, &QThread::quit);
//     connect(m_worker, &Worker::ConversionWorker::conversionFinished, m_worker, &QObject::deleteLater);
//     connect(m_workerThread, &QThread::finished, m_workerThread, &QObject::deleteLater);

//     m_startBtn->setEnabled(false);
//     m_cancelBtn->setEnabled(true);
//     m_progressBar->setValue(0);

//     m_workerThread->start();
// }


void MainWindow::startConversion() {
    if (m_inputPathEdit->text().isEmpty() || m_outputPathEdit->text().isEmpty()) {
        QMessageBox::warning(this, "Missing Path", "Please select valid input and output file paths.");
        return;
    }

    Core::TranscodeOptions options;
    options.inputFilePath = m_inputPathEdit->text().toStdString();
    options.outputFilePath = m_outputPathEdit->text().toStdString();

    // Default Video Options
    options.video.enableVideo = true;
    options.video.codecId = AV_CODEC_ID_H264;
    options.video.bitRate = m_presetCombo->currentData().toInt();

    // Default Audio Options (AAC 48kHz Stereo @ 192kbps)
    options.audio.enableAudio = true;
    options.audio.codecId = AV_CODEC_ID_AAC;
    options.audio.sampleRate = 48000;
    options.audio.channels = 2;
    options.audio.bitRate = 192000;
    options.audio.sampleFmt = AV_SAMPLE_FMT_FLTP;

    m_workerThread = new QThread(this);
    m_worker = new Worker::ConversionWorker(options);
    m_worker->moveToThread(m_workerThread);

    connect(m_workerThread, &QThread::started, m_worker, &Worker::ConversionWorker::process);
    connect(m_worker, &Worker::ConversionWorker::progressUpdated, this, &MainWindow::onProgressUpdated);
    connect(m_worker, &Worker::ConversionWorker::statusMessage, this, &MainWindow::onStatusMessage);
    connect(m_worker, &Worker::ConversionWorker::conversionFinished, this, &MainWindow::onConversionFinished);

    connect(m_worker, &Worker::ConversionWorker::conversionFinished, m_workerThread, &QThread::quit);
    connect(m_worker, &Worker::ConversionWorker::conversionFinished, m_worker, &QObject::deleteLater);
    connect(m_workerThread, &QThread::finished, m_workerThread, &QObject::deleteLater);

    m_startBtn->setEnabled(false);
    m_cancelBtn->setEnabled(true);
    m_progressBar->setValue(0);

    m_workerThread->start();
}

void MainWindow::cancelConversion() {
    if (m_worker) {
        m_worker->cancel();
    }
}

void MainWindow::onProgressUpdated(int percentage) {
    m_progressBar->setValue(percentage);
}

void MainWindow::onStatusMessage(const QString& message) {
    m_statusLabel->setText(message);
}

void MainWindow::onConversionFinished(bool success, const QString& message) {
    m_startBtn->setEnabled(true);
    m_cancelBtn->setEnabled(false);
    m_statusLabel->setText(message);

    if (success) {
        QMessageBox::information(this, "Success", message);
    } else {
        QMessageBox::warning(this, "Error", message);
    }
}