#include "MainWindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QFileInfo>
#include <QGroupBox>

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
    setWindowTitle("Media_TranscodeX - Desktop Media Converter & Inspector");
    resize(800, 650);

    QWidget *centralWidget = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

    // Input File Row
    QHBoxLayout *inputLayout = new QHBoxLayout();
    m_inputPathEdit = new QLineEdit(this);
    m_inputPathEdit->setPlaceholderText("Select source video/audio file...");
    QPushButton *browseInputBtn = new QPushButton("Browse...", this);
    connect(browseInputBtn, &QPushButton::clicked, this, &MainWindow::browseInputFile);
    connect(m_inputPathEdit, &QLineEdit::editingFinished, this, [this]() {
        inspectFile(m_inputPathEdit->text());
    });
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

    // Media Inspector Section
    QGroupBox *inspectorGroup = new QGroupBox("🔍 Media Streams & Track Manager", this);
    QVBoxLayout *inspectorLayout = new QVBoxLayout(inspectorGroup);

    m_infoTree = new QTreeWidget(this);
    m_infoTree->setHeaderLabels({"Property / Track", "Value / Details"});
    m_infoTree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_infoTree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    inspectorLayout->addWidget(m_infoTree);

    // Stream Modification Buttons
    QHBoxLayout *trackBtnLayout = new QHBoxLayout();
    m_addAudioBtn = new QPushButton("➕ Add Audio Track...", this);
    m_addSubtitleBtn = new QPushButton("➕ Add Subtitle Track...", this);
    m_removeTrackBtn = new QPushButton("❌ Remove / Disable Track", this);

    connect(m_addAudioBtn, &QPushButton::clicked, this, &MainWindow::addAudioTrack);
    connect(m_addSubtitleBtn, &QPushButton::clicked, this, &MainWindow::addSubtitleTrack);
    connect(m_removeTrackBtn, &QPushButton::clicked, this, &MainWindow::removeSelectedTrack);

    trackBtnLayout->addWidget(m_addAudioBtn);
    trackBtnLayout->addWidget(m_addSubtitleBtn);
    trackBtnLayout->addWidget(m_removeTrackBtn);
    inspectorLayout->addLayout(trackBtnLayout);

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

    m_statusLabel = new QLabel("Ready. Select a file to inspect metadata and manage tracks.", this);
    m_statusLabel->setAlignment(Qt::AlignCenter);

    // Action Buttons
    QHBoxLayout *btnLayout = new QHBoxLayout();
    m_startBtn = new QPushButton("Start Transcoding & Multiplexing", this);
    m_cancelBtn = new QPushButton("Cancel", this);
    m_cancelBtn->setEnabled(false);

    connect(m_startBtn, &QPushButton::clicked, this, &MainWindow::startConversion);
    connect(m_cancelBtn, &QPushButton::clicked, this, &MainWindow::cancelConversion);

    btnLayout->addWidget(m_startBtn);
    btnLayout->addWidget(m_cancelBtn);

    // Assemble Layout
    mainLayout->addLayout(inputLayout);
    mainLayout->addLayout(outputLayout);
    mainLayout->addWidget(inspectorGroup);
    mainLayout->addLayout(presetLayout);
    mainLayout->addWidget(m_progressBar);
    mainLayout->addWidget(m_statusLabel);
    mainLayout->addLayout(btnLayout);

    setCentralWidget(centralWidget);
}

void MainWindow::browseInputFile() {
    QString path = QFileDialog::getOpenFileName(this, "Select Source File", "", "Media Files (*.mp4 *.mkv *.avi *.webm *.mp3 *.m4a *.aac *.flac *.srt *.vtt);;All Files (*)");
    if (!path.isEmpty()) {
        m_inputPathEdit->setText(path);
        QFileInfo info(path);
        m_outputPathEdit->setText(info.absolutePath() + "/" + info.completeBaseName() + "_converted.mp4");
        inspectFile(path);
    }
}

void MainWindow::browseOutputFile() {
    QString path = QFileDialog::getSaveFileName(this, "Save Converted File", "", "MP4 Video (*.mp4);;MKV Video (*.mkv);;All Files (*)");
    if (!path.isEmpty()) {
        m_outputPathEdit->setText(path);
    }
}

void MainWindow::inspectFile(const QString& filePath) {
    m_infoTree->clear();
    m_audioCategoryItem = nullptr;
    m_subtitleCategoryItem = nullptr;

    if (filePath.isEmpty() || !QFile::exists(filePath)) {
        return;
    }

    Core::MediaDemuxer demuxer;
    if (!demuxer.openFile(filePath.toStdString())) {
        QTreeWidgetItem *errItem = new QTreeWidgetItem(m_infoTree);
        errItem->setText(0, "Error");
        errItem->setText(1, "Unable to parse media container.");
        return;
    }

    const Core::MediaInfo& info = demuxer.getInfo();

    auto addProp = [](QTreeWidgetItem* parent, const QString& name, const QString& val) {
        QTreeWidgetItem *item = new QTreeWidgetItem(parent);
        item->setText(0, name);
        item->setText(1, val);
    };

    // 1. Container & General Info
    QTreeWidgetItem *containerCategory = new QTreeWidgetItem(m_infoTree);
    containerCategory->setText(0, "📁 Container Info");
    containerCategory->setExpanded(true);

    addProp(containerCategory, "Format", QString::fromStdString(info.formatName + " (" + info.formatLongName + ")"));
    addProp(containerCategory, "Duration", QString("%1 sec").arg(info.durationSeconds, 0, 'f', 2));
    if (info.bitRate > 0) {
        addProp(containerCategory, "Overall Bitrate", QString("%1 kbps").arg(info.bitRate / 1000));
    }

    // 2. Metadata Tags (Description, Title, Artist, etc.)
    if (!info.metadataTags.empty()) {
        QTreeWidgetItem *tagsCategory = new QTreeWidgetItem(m_infoTree);
        tagsCategory->setText(0, "🏷️ Description & Tags");
        tagsCategory->setExpanded(true);

        for (const auto& pair : info.metadataTags) {
            addProp(tagsCategory, QString::fromStdString(pair.first), QString::fromStdString(pair.second));
        }
    }

    // 3. Video Streams
    if (!info.videoTracks.empty()) {
        QTreeWidgetItem *videoCategory = new QTreeWidgetItem(m_infoTree);
        videoCategory->setText(0, QString("🎬 Video Tracks (%1)").arg(info.videoTracks.size()));
        videoCategory->setExpanded(true);

        for (const auto& v : info.videoTracks) {
            QTreeWidgetItem *vTrack = new QTreeWidgetItem(videoCategory);
            vTrack->setText(0, QString("Stream #%1").arg(v.index));
            vTrack->setText(1, QString::fromStdString(v.codecName + " (" + v.codecLongName + ")"));
            vTrack->setExpanded(true);

            addProp(vTrack, "Resolution", QString("%1x%2").arg(v.width).arg(v.height));
            if (v.fps > 0) addProp(vTrack, "Frame Rate", QString("%1 fps").arg(v.fps, 0, 'f', 2));
            addProp(vTrack, "Pixel Format", QString::fromStdString(v.pixelFormat));
            if (v.bitRate > 0) addProp(vTrack, "Bitrate", QString("%1 kbps").arg(v.bitRate / 1000));
            if (!v.language.empty()) addProp(vTrack, "Language", QString::fromStdString(v.language));
            if (!v.title.empty()) addProp(vTrack, "Title", QString::fromStdString(v.title));
        }
    }

    // 4. Audio Streams
    m_audioCategoryItem = new QTreeWidgetItem(m_infoTree);
    m_audioCategoryItem->setText(0, QString("🎵 Audio Tracks (%1)").arg(info.audioTracks.size()));
    m_audioCategoryItem->setExpanded(true);

    for (const auto& a : info.audioTracks) {
        QTreeWidgetItem *aTrack = new QTreeWidgetItem(m_audioCategoryItem);
        aTrack->setFlags(aTrack->flags() | Qt::ItemIsUserCheckable);
        aTrack->setCheckState(0, Qt::Checked);
        aTrack->setText(0, QString("Internal Stream #%1").arg(a.index));
        aTrack->setText(1, QString::fromStdString(a.codecName + " (" + a.codecLongName + ")"));
        aTrack->setData(0, Qt::UserRole + 1, static_cast<int>(Core::TrackType::Audio));
        aTrack->setData(0, Qt::UserRole + 2, static_cast<int>(Core::TrackSourceType::Internal));
        aTrack->setData(0, Qt::UserRole + 3, a.index);
        aTrack->setExpanded(true);

        addProp(aTrack, "Channels", QString("%1 ch").arg(a.channels));
        addProp(aTrack, "Sample Rate", QString("%1 Hz").arg(a.sampleRate));
        if (a.bitRate > 0) addProp(aTrack, "Bitrate", QString("%1 kbps").arg(a.bitRate / 1000));
        if (!a.language.empty()) addProp(aTrack, "Language", QString::fromStdString(a.language));
        if (!a.title.empty()) addProp(aTrack, "Title", QString::fromStdString(a.title));
    }

    // 5. Subtitle Streams
    m_subtitleCategoryItem = new QTreeWidgetItem(m_infoTree);
    m_subtitleCategoryItem->setText(0, QString("💬 Subtitle Tracks (%1)").arg(info.subtitleTracks.size()));
    m_subtitleCategoryItem->setExpanded(true);

    for (const auto& s : info.subtitleTracks) {
        QTreeWidgetItem *sTrack = new QTreeWidgetItem(m_subtitleCategoryItem);
        sTrack->setFlags(sTrack->flags() | Qt::ItemIsUserCheckable);
        sTrack->setCheckState(0, Qt::Checked);
        sTrack->setText(0, QString("Internal Stream #%1").arg(s.index));
        sTrack->setText(1, QString::fromStdString(s.codecName + " (" + s.codecLongName + ")"));
        sTrack->setData(0, Qt::UserRole + 1, static_cast<int>(Core::TrackType::Subtitle));
        sTrack->setData(0, Qt::UserRole + 2, static_cast<int>(Core::TrackSourceType::Internal));
        sTrack->setData(0, Qt::UserRole + 3, s.index);
        sTrack->setExpanded(true);

        if (!s.language.empty()) addProp(sTrack, "Language", QString::fromStdString(s.language));
        if (!s.title.empty()) addProp(sTrack, "Title", QString::fromStdString(s.title));
        if (s.isDefault) addProp(sTrack, "Default Flag", "Yes");
        if (s.isForced) addProp(sTrack, "Forced Flag", "Yes");
    }
}

void MainWindow::addAudioTrack() {
    QString path = QFileDialog::getOpenFileName(this, "Select External Audio File", "", "Audio Files (*.mp3 *.m4a *.aac *.wav *.flac *.ogg);;All Files (*)");
    if (path.isEmpty()) return;

    bool ok = false;
    double offset = QInputDialog::getDouble(this, "Audio Start Offset", "Start Offset in External Audio (seconds):", 0.0, 0.0, 86400.0, 1, &ok);
    if (!ok) offset = 0.0;

    double duration = QInputDialog::getDouble(this, "Audio Max Duration", "Max Audio Duration in seconds (0 = match main video length):", 0.0, 0.0, 86400.0, 1, &ok);
    if (!ok) duration = 0.0;

    if (!m_audioCategoryItem) {
        m_audioCategoryItem = new QTreeWidgetItem(m_infoTree);
        m_audioCategoryItem->setText(0, "🎵 Audio Tracks");
        m_audioCategoryItem->setExpanded(true);
    }

    QFileInfo info(path);
    QTreeWidgetItem *aTrack = new QTreeWidgetItem(m_audioCategoryItem);
    aTrack->setFlags(aTrack->flags() | Qt::ItemIsUserCheckable);
    aTrack->setCheckState(0, Qt::Checked);
    aTrack->setText(0, QString("External Audio: %1").arg(info.fileName()));
    
    QString details = path;
    if (offset > 0 || duration > 0) {
        details += QString(" (Offset: %1s, Duration: %2)").arg(offset).arg(duration > 0 ? QString("%1s").arg(duration) : "Auto Video Match");
    } else {
        details += " (Auto Video Match)";
    }
    aTrack->setText(1, details);

    aTrack->setData(0, Qt::UserRole + 1, static_cast<int>(Core::TrackType::Audio));
    aTrack->setData(0, Qt::UserRole + 2, static_cast<int>(Core::TrackSourceType::External));
    aTrack->setData(0, Qt::UserRole + 3, path);
    aTrack->setData(0, Qt::UserRole + 4, offset);
    aTrack->setData(0, Qt::UserRole + 5, duration);
}

void MainWindow::addSubtitleTrack() {
    QString path = QFileDialog::getOpenFileName(this, "Select External Subtitle File", "", "Subtitle Files (*.srt *.vtt *.ass);;All Files (*)");
    if (path.isEmpty()) return;

    if (!m_subtitleCategoryItem) {
        m_subtitleCategoryItem = new QTreeWidgetItem(m_infoTree);
        m_subtitleCategoryItem->setText(0, "💬 Subtitle Tracks");
        m_subtitleCategoryItem->setExpanded(true);
    }

    QFileInfo info(path);
    QTreeWidgetItem *sTrack = new QTreeWidgetItem(m_subtitleCategoryItem);
    sTrack->setFlags(sTrack->flags() | Qt::ItemIsUserCheckable);
    sTrack->setCheckState(0, Qt::Checked);
    sTrack->setText(0, QString("External Subtitle: %1").arg(info.fileName()));
    sTrack->setText(1, path);
    sTrack->setData(0, Qt::UserRole + 1, static_cast<int>(Core::TrackType::Subtitle));
    sTrack->setData(0, Qt::UserRole + 2, static_cast<int>(Core::TrackSourceType::External));
    sTrack->setData(0, Qt::UserRole + 3, path);
}

void MainWindow::removeSelectedTrack() {
    QTreeWidgetItem *item = m_infoTree->currentItem();
    if (!item) return;

    QVariant sourceTypeVar = item->data(0, Qt::UserRole + 2);
    if (!sourceTypeVar.isValid()) return;

    Core::TrackSourceType sourceType = static_cast<Core::TrackSourceType>(sourceTypeVar.toInt());
    if (sourceType == Core::TrackSourceType::External) {
        delete item;
    } else {
        item->setCheckState(0, Qt::Unchecked);
    }
}

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

    // Default Audio Encoding Settings
    options.audio.enableAudio = true;
    options.audio.codecId = AV_CODEC_ID_AAC;
    options.audio.sampleRate = 48000;
    options.audio.channels = 2;
    options.audio.bitRate = 192000;
    options.audio.sampleFmt = AV_SAMPLE_FMT_FLTP;

    // Scan Audio Tracks
    if (m_audioCategoryItem) {
        for (int i = 0; i < m_audioCategoryItem->childCount(); ++i) {
            QTreeWidgetItem *child = m_audioCategoryItem->child(i);
            if (child->checkState(0) == Qt::Checked) {
                Core::TrackSelection sel;
                sel.type = Core::TrackType::Audio;
                sel.enabled = true;
                sel.sourceType = static_cast<Core::TrackSourceType>(child->data(0, Qt::UserRole + 2).toInt());
                if (sel.sourceType == Core::TrackSourceType::Internal) {
                    sel.sourceStreamIndex = child->data(0, Qt::UserRole + 3).toInt();
                } else {
                    sel.externalFilePath = child->data(0, Qt::UserRole + 3).toString().toStdString();
                    sel.startTimeSec = child->data(0, Qt::UserRole + 4).toDouble();
                    sel.maxDurationSec = child->data(0, Qt::UserRole + 5).toDouble();
                }
                options.selectedAudioTracks.push_back(sel);
            }
        }
    }

    // Scan Subtitle Tracks
    if (m_subtitleCategoryItem) {
        for (int j = 0; j < m_subtitleCategoryItem->childCount(); ++j) {
            QTreeWidgetItem *child = m_subtitleCategoryItem->child(j);
            if (child->checkState(0) == Qt::Checked) {
                Core::TrackSelection sel;
                sel.type = Core::TrackType::Subtitle;
                sel.enabled = true;
                sel.sourceType = static_cast<Core::TrackSourceType>(child->data(0, Qt::UserRole + 2).toInt());
                if (sel.sourceType == Core::TrackSourceType::Internal) {
                    sel.sourceStreamIndex = child->data(0, Qt::UserRole + 3).toInt();
                } else {
                    sel.externalFilePath = child->data(0, Qt::UserRole + 3).toString().toStdString();
                }
                options.selectedSubtitleTracks.push_back(sel);
            }
        }
    }

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