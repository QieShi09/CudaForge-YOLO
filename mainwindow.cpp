#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "videowidget.h"
#include <QFileDialog>
#include <QEvent>
#include <QDateTime>
#include <QDir>
#include <QTimer>
#include <QMetaType>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QSizeGrip>
#include <QStyle>
#include <QDebug>
#include <QInputDialog>
#include <QPushButton>
#include <QVBoxLayout>
#include "../engine/TRTDetector.hpp"
#include "src/core/InputFrameArenaStore.hpp"
#include "src/core/TensorArenaManager.hpp"
#include "src/core/SlotPool.hpp"
#include "src/core/DetectionResults.hpp"
#include "src/core/ChannelResultQueue.hpp"
#include "src/core/PipelineStats.hpp"
#include "src/core/NvtxUtils.hpp"
#include <chrono>
#include <QFile>
#include <algorithm>
#include <QTextStream>
#include <cuda_runtime.h>
#include <nvtx3/nvToolsExt.h>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    ui->btn_snapshot->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));

    setWindowFlags((windowFlags() | Qt::FramelessWindowHint) & ~Qt::WindowContextHelpButtonHint);
    setAttribute(Qt::WA_StyledBackground, true);

    m_customTitleBar = new QWidget(this);
    m_customTitleBar->setObjectName("customTitleBarMain");
    auto *titleLayout = new QHBoxLayout(m_customTitleBar);
    titleLayout->setContentsMargins(12, 6, 8, 8);
    titleLayout->setSpacing(8);

    m_mainTitleLabel = new QLabel(windowTitle(), m_customTitleBar);
    m_mainTitleLabel->setObjectName("mainTitleLabel");

    m_btnTitleMin = new QPushButton("—", m_customTitleBar);
    m_btnTitleMax = new QPushButton("▢", m_customTitleBar);
    m_btnTitleClose = new QPushButton("×", m_customTitleBar);
    m_btnTitleMin->setObjectName("titleMinButton");
    m_btnTitleMax->setObjectName("titleMaxButton");
    m_btnTitleClose->setObjectName("titleCloseButtonMain");

    for (auto *btn : {m_btnTitleMin, m_btnTitleMax, m_btnTitleClose}) {
        btn->setFixedSize(24, 24);
        btn->setCursor(Qt::PointingHandCursor);
    }

    titleLayout->addWidget(m_mainTitleLabel);
    titleLayout->addStretch();
    titleLayout->addWidget(m_btnTitleMin);
    titleLayout->addWidget(m_btnTitleMax);
    titleLayout->addWidget(m_btnTitleClose);

    connect(m_btnTitleMin, &QPushButton::clicked, this, &QWidget::showMinimized);
    connect(m_btnTitleMax, &QPushButton::clicked, this, [this]() {
        if (isMaximized()) showNormal(); else showMaximized();
    });
    connect(m_btnTitleClose, &QPushButton::clicked, this, &QWidget::close);

    m_customTitleBar->installEventFilter(this);
    m_mainTitleLabel->installEventFilter(this);
    setMenuWidget(m_customTitleBar);

    ui->statusbar->setSizeGripEnabled(true);
    auto *mainResizeGrip = new QSizeGrip(this);
    mainResizeGrip->setStyleSheet("QSizeGrip { width: 14px; height: 14px; }");
    ui->statusbar->addPermanentWidget(mainResizeGrip, 0);

    // 注册 void* 类型，以便在跨线程信号槽中正确传递
    qRegisterMetaType<void*>("void*");

    // 设置视频网格的间距，让画面更紧凑，更有监控室的感觉
    ui->gridLayout_video->setSpacing(8);
    ui->gridLayout_video->setContentsMargins(6, 6, 6, 6);

    // 初始化时，手动触发一次布局更新，确保界面显示正确的默认布局（2x2）
    // 这样程序启动时就会自动创建好 4 个视频窗口
    on_comboBox_layout_currentIndexChanged(ui->comboBox_layout->currentIndex());

    // 初始化：隐藏分析模式的侧边栏控件
    ui->widget_sidebar_analysis->setVisible(false);

    // 隐藏侧边栏的视频控制组（网格模式下不显示）
    ui->groupBox_video_controls->setVisible(false);
    // 详情页的单独 FPS 控件已移除，统一使用默认行为（网格 30 FPS，详情 60 FPS）

    // 初始化按钮状态：未开始检测时，停止按钮不可用
    ui->btn_stop->setEnabled(false);

    // 加载持久化设置
    {
        auto s = AdvancedSettingsDialog::loadFromDisk();
        m_baseSlots       = s.baseSlots;
        m_inputArenaFrames = std::max(16, s.inputArenaFrames);
        m_inferenceStreams = std::max(1, s.inferenceStreams);
        m_workerMaxBatch  = s.workerMaxBatch;
        m_modelPath       = s.modelPath;
        m_classesPath     = s.classesPath;
    }

    // 高级设置按钮（动态添加到 btn_stop 后面）
    {
        m_btnAdvancedSettings = new QPushButton("⚙ Advanced Settings", this);
        m_btnAdvancedSettings->setMinimumHeight(36);
        m_btnAdvancedSettings->setStyleSheet(
            "QPushButton {"
            " background: rgba(232, 243, 238, 0.98);"
            " color: #2f4652;"
            " border: 1px solid #b9cdc6;"
            " border-radius: 7px;"
            " padding: 6px 12px;"
            " font-weight: 700;"
            "}"
            "QPushButton:hover {"
            " background: rgba(221, 236, 230, 0.98);"
            " border: 1px solid #9ebdb3;"
            "}");
        // 插入到 btn_stop 和 verticalSpacer 之间
        auto* sidebar = ui->btn_stop->parentWidget();
        if (auto* vl = qobject_cast<QVBoxLayout*>(sidebar->layout())) {
            int stopIdx = vl->indexOf(ui->btn_stop);
            vl->insertWidget(stopIdx + 1, m_btnAdvancedSettings);
        }
        connect(m_btnAdvancedSettings, &QPushButton::clicked,
                this, &MainWindow::openAdvancedSettings);
    }

    // 初始化显示管理器
    m_displayManager = new DisplayManager(this);

    // 初始化防抖定时器
    m_inputDebounceTimer = new QTimer(this);
    m_inputDebounceTimer->setSingleShot(true);

    // 初始化检测表格更新定时器
    m_detectionUpdateTimer = new QTimer(this);
    m_detectionUpdateTimer->setInterval(300);
    connect(m_detectionUpdateTimer, &QTimer::timeout, this, [this](){
        auto drainChannelResult = [this](int channel_id) {
            ChannelResultQueue::Item latest;
            if (!ChannelResultQueue::getInstance().popLatest(channel_id, latest)) return;
            if (latest.epoch > 0) {
                DetectionResults::getInstance().updateIfCurrent(channel_id, latest.epoch, std::move(latest.detections));
            } else {
                DetectionResults::getInstance().update(channel_id, std::move(latest.detections));
            }
        };

        for (auto it = m_decoders.constBegin(); it != m_decoders.constEnd(); ++it) {
            drainChannelResult(it.key());
        }
        for (auto it = m_channelSettings.constBegin(); it != m_channelSettings.constEnd(); ++it) {
            if (!m_decoders.contains(it.key())) {
                drainChannelResult(it.key());
            }
        }

        // 仅当界面处在 Analysis tab（或需要显示时）才更新
        if (ui->tabWidget_details->currentWidget() == ui->tab_detections) {
            int ch = m_currentChannelIndex;
            auto dets = DetectionResults::getInstance().get(ch);
            ui->tableWidget_detections->setRowCount(0);
            for (const auto &d : dets) {
                int row = ui->tableWidget_detections->rowCount();
                ui->tableWidget_detections->insertRow(row);
                QString name = QString::number(d.class_id);
                if (static_cast<size_t>(d.class_id) < m_classNames.size()) name = m_classNames[d.class_id];
                ui->tableWidget_detections->setItem(row, 0, new QTableWidgetItem(name));
                ui->tableWidget_detections->setItem(row, 1, new QTableWidgetItem(QString::number(d.conf, 'f', 2)));
                ui->tableWidget_detections->setItem(row, 2, new QTableWidgetItem(QString("[%1, %2, %3, %4]")
                                                                                    .arg(d.x).arg(d.y).arg(d.w).arg(d.h)));
            }
        }
    });
    m_detectionUpdateTimer->start();

    // 进度条信号连接（拖动释放时跳转）
    connect(ui->slider_seek, &QSlider::sliderReleased, this, &MainWindow::on_slider_seek_sliderReleased);

    // 初始化进度条更新定时器（每 500ms 更新一次进度）
    m_progressUpdateTimer = new QTimer(this);
    connect(m_progressUpdateTimer, &QTimer::timeout, this, [this](){
        if (!m_isAnalysisMode) return;
        // 使用独立解码器
        if (!m_analysisDecoder) return;
        
        int64_t duration = m_analysisDecoder->getDuration();
        if (duration <= 0) return; // 直播流无时长
        
        // 获取当前播放位置并更新进度条
        int64_t pos = m_analysisDecoder->getCurrentPosition();
        // 仅在用户未拖动时更新，避免冲突
        if (!ui->slider_seek->isSliderDown()) {
            ui->slider_seek->setValue(static_cast<int>(pos * 1000 / duration));
        }
    });

    // 打印一条欢迎日志
    logSystemMessage("System initialized. Ready for monitoring.", 0);
}

MainWindow::~MainWindow()
{
    // 清理独立解码器
    cleanupAnalysisDecoder();
    
    // 停止所有线程
    for (auto id : m_decoders.keys()) {
        stopChannel(id);
    }
    stopDetection();
    TRTDetector::getInstance().shutdown();
    delete ui;
}

int MainWindow::activeConfiguredChannels() const
{
    int cnt = 0;
    for (auto it = m_channelSettings.constBegin(); it != m_channelSettings.constEnd(); ++it) {
        if (!it.value().sourcePath.trimmed().isEmpty()) ++cnt;
    }
    if (cnt <= 0) cnt = std::max(1, static_cast<int>(m_decoders.size()));
    return std::max(1, cnt);
}

int MainWindow::recommendedSlotCount(int effectiveBatch) const
{
    (void)effectiveBatch;
    return std::clamp(std::max(1, m_baseSlots), 1, 256);
}

// ============ 辅助函数 ============

template<typename Func>
void MainWindow::forEachVideoWidget(Func callback)
{
    for (int i = 0; i < ui->gridLayout_video->count(); ++i) {
        QLayoutItem *item = ui->gridLayout_video->itemAt(i);
        if (QWidget *w = item ? item->widget() : nullptr) {
            if (VideoWidget *v = qobject_cast<VideoWidget*>(w)) {
                callback(v);
            }
        }
    }
}

VideoWidget* MainWindow::findVideoWidget(int channelId)
{
    for (int i = 0; i < ui->gridLayout_video->count(); ++i) {
        QLayoutItem *item = ui->gridLayout_video->itemAt(i);
        if (QWidget *w = item ? item->widget() : nullptr) {
            if (VideoWidget *v = qobject_cast<VideoWidget*>(w)) {
                if (v->property("channelIndex").toInt() == channelId) {
                    return v;
                }
            }
        }
    }
    return nullptr;
}

void MainWindow::cleanupAnalysisDecoder()
{
    if (!m_analysisDecoder) return;
    
    m_analysisDecoder->stopDecoding();
    
    if (m_analysisThread) {
        m_analysisThread->quit();
        m_analysisThread->wait();
        delete m_analysisThread;
        m_analysisThread = nullptr;
    }
    
    if (m_analysisFrameQueue) {
        m_analysisFrameQueue->stop();
    }
    
    if (m_analysisDisplayThread) {
        m_analysisDisplayThread->quit();
        m_analysisDisplayThread->wait();
        delete m_analysisDisplayThread;
        m_analysisDisplayThread = nullptr;
    }
    
    m_analysisWorker = nullptr; // 由线程自动删除
    delete m_analysisDecoder;
    m_analysisDecoder = nullptr;
    delete m_analysisFrameQueue;
    m_analysisFrameQueue = nullptr;
}

// ============ 槽函数 ============

void MainWindow::on_comboBox_layout_currentIndexChanged(int index)
{
    int rows = 2;
    int cols = 2;
    QString title = "Real-time Monitoring";

    // 新的排序逻辑:
    // Index 0: 1x1 Single Focus
    // Index 1: 2x2 Quad View (Default)
    // Index 2: 3x3 Grid View
    switch (index) {
    case 0: rows = 1; cols = 1; title = "Real-time Monitoring (1-CH)"; break;
    case 1: rows = 2; cols = 2; title = "Real-time Monitoring (4-CH)"; break;
    case 2: rows = 3; cols = 3; title = "Real-time Monitoring (9-CH)"; break;
    default: break;
    }

    // 1. 动态更新左上角 GroupBox 的标题
    ui->groupBox_monitor->setTitle(title);

    // 计算新网格的总通道数
    int totalChannels = rows * cols;

    // 停止并清理被移除的通道（在删除控件之前停止解码器，避免后台继续推送帧）
    QList<int> toRemove;
    for (auto it = m_decoders.begin(); it != m_decoders.end(); ++it) {
        int ch = it.key();
        if (ch >= totalChannels) toRemove.append(ch);
    }
    for (int ch : toRemove) {
        stopChannel(ch);
        // 移除通道时清理该通道的检测结果（保守策略）
        DetectionResults::getInstance().clear(ch);
        ChannelResultQueue::getInstance().clearChannel(ch);
        if (m_channelSettings.contains(ch)) {
            m_channelSettings[ch].sourcePath.clear();
        }
        logSystemMessage(QString("Channel %1 removed due to layout change.").arg(ch + 1), 1);
    }

    // 先创建视频网格，确保后续设置选中项时，控件已经存在
    updateVideoGrid(rows, cols);

    // 切换布局时清理所有通道的数据源和显示，防止残留摄像头路径或画面
    // 停止所有仍在运行的解码器并清空配置中的 sourcePath
    QList<int> runningChannels = m_decoders.keys();
    for (int ch : runningChannels) {
        stopChannel(ch);
    }
    for (auto it = m_channelSettings.begin(); it != m_channelSettings.end(); ++it) {
        it.value().sourcePath.clear();
    }
    // 清空右侧输入框（当前选择的通道）以保证 UI 不显示已移除的设备路径
    ui->lineEdit_source->clear();
    // 清空所有 VideoWidget 的显示，避免画面残留
    forEachVideoWidget([](VideoWidget* v){ v->clear(); });

    // 2. 更新通道选择下拉框的内容
    ui->comboBox_channelSelect->blockSignals(true); // 暂时屏蔽信号，防止频繁触发加载
    ui->comboBox_channelSelect->clear();
    for (int i = 0; i < totalChannels; ++i) {
        ui->comboBox_channelSelect->addItem(QString("CAM %1").arg(i + 1, 2, 10, QChar('0')));
    }
    ui->comboBox_channelSelect->blockSignals(false);
    
    // 恢复之前的选择（如果还在范围内），否则选第一个
    if (m_currentChannelIndex >= totalChannels) m_currentChannelIndex = 0;
    ui->comboBox_channelSelect->setCurrentIndex(m_currentChannelIndex);

    // （已在上文处理）移除多余通道的逻辑已执行，无需重复。
}

void MainWindow::updateVideoGrid(int rows, int cols)
{
    // 1. 清除旧的控件
    QLayoutItem *item;
    while ((item = ui->gridLayout_video->takeAt(0)) != nullptr) {
        if (QWidget *widget = item->widget()) {
            delete widget; // 删除 Label 控件
        }
        delete item; // 删除布局项
    }

    // 根据网格数量计算每个视频窗口的建议分辨率
    // 1x1 -> 1920x1080 (全高清)
    // 2x2 -> 960x540 (半高清)
    // 3x3 -> 640x360 (标清)
    int targetW = 1920 / cols;
    int targetH = 1080 / rows;

    // 2. 动态创建新网格
    int count = 1;
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            // 使用自定义的 OpenGL 控件
            VideoWidget *video = new VideoWidget(this);
            
            // 安装事件过滤器，以便捕获点击事件
            video->installEventFilter(this);
            video->setProperty("channelIndex", count - 1); // 存储通道索引 (0-based)
            
            // 连接关闭信号，处理路径清理逻辑
            connect(video, &VideoWidget::closeRequested, this, [this, count](){
                this->onChannelCloseRequested(count - 1);
            });
            
            // 连接重播信号（网格模式下视频源播放结束后）
            connect(video, &VideoWidget::replayRequested, this, &MainWindow::onVideoReplayRequested);

            // 设置期望的解码/显示尺寸
            // VideoWidget 内部应将此参数传递给 VideoDecoder::setDisplaySize
            video->setProperty("targetWidth", targetW);
            video->setProperty("targetHeight", targetH);
            
            // 创建时立即初始化选中状态，防止因下拉框索引未变而不触发更新信号导致第一个窗口无绿框
            if (count - 1 == m_currentChannelIndex) {
                video->setSelected(true);
            }
            
            // 设置初始文字
            video->setChannelId(count - 1);
            
            // 添加到网格布局
            ui->gridLayout_video->addWidget(video, r, c);
            
            count++;
        }
    }
    
    // 刷新一次当前通道的设置显示
    loadChannelSettings(m_currentChannelIndex);
}

void MainWindow::onChannelCloseRequested(int index)
{
    // 1. 停止后台解码
    stopChannel(index);
    
    // 2. [关键修复] 清理数据源记录
    m_channelSettings[index].sourcePath.clear();
    
    // 3. 如果当前正在看这个频道，同步清理输入框
    if (index == m_currentChannelIndex) {
        ui->lineEdit_source->clear();
    }
    logSystemMessage(QString("Channel %1 cleared.").arg(index + 1));
}

// 事件过滤器：处理视频窗口的点击
bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_customTitleBar || watched == m_mainTitleLabel) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto *me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                m_titleDragging = true;
                m_titleDragOffset = me->globalPosition().toPoint() - frameGeometry().topLeft();
                return true;
            }
        } else if (event->type() == QEvent::MouseMove && m_titleDragging) {
            auto *me = static_cast<QMouseEvent*>(event);
            move(me->globalPosition().toPoint() - m_titleDragOffset);
            return true;
        } else if (event->type() == QEvent::MouseButtonRelease) {
            m_titleDragging = false;
            return true;
        }
    }

    // 双击进入详情模式
    if (event->type() == QEvent::MouseButtonDblClick) {
        VideoWidget *video = qobject_cast<VideoWidget*>(watched);
        if (video && video->property("channelIndex").isValid()) {
            int index = video->property("channelIndex").toInt();
            enterAnalysisMode(index);
            return true;
        }
    }

    if (event->type() == QEvent::MouseButtonPress) {
        // 检查被点击的对象是否是我们创建的 VideoWidget
        VideoWidget *video = qobject_cast<VideoWidget*>(watched);
        if (video && video->property("channelIndex").isValid()) {
            int index = video->property("channelIndex").toInt();
            
            // 更新右侧的通道选择框，这会自动触发 on_comboBox_channelSelect_currentIndexChanged
            ui->comboBox_channelSelect->setCurrentIndex(index);
            
            // 直接在日志和状态栏反馈
            logSystemMessage(QString("Selected CAM %1").arg(index + 1));
            
            return true; // 事件已处理
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::on_btn_browse_clicked()
{
    int lockedIndex = m_currentChannelIndex; // 锁定当前操作的频道索引
    int typeIndex = ui->comboBox_sourceType->currentIndex();
    QString filter;
    QString title;

    // USB 摄像头模式：扫描设备并让用户选择
    if (typeIndex == 2) {
        QDir devDir("/dev");
        QStringList filters;
        filters << "video*"; // 扫描 /dev/video0, /dev/video1 ...
        QStringList devices = devDir.entryList(filters, QDir::System | QDir::NoDotAndDotDot);
        
        if (devices.isEmpty()) {
            logSystemMessage("No video devices found in /dev/", 2);
            return;
        }

        bool ok;
        QString item = QInputDialog::getItem(this, tr("Select Camera"), 
                                             tr("Device:"), devices, 0, false, &ok);
        if (ok && !item.isEmpty()) {
            QString fileName = "/dev/" + item;
            ui->lineEdit_source->blockSignals(true); 
            ui->lineEdit_source->setText(fileName);
            ui->lineEdit_source->blockSignals(false);
            m_inputDebounceTimer->stop();

            m_channelSettings[lockedIndex].sourcePath = fileName;
            stopChannel(lockedIndex);
            startChannel(lockedIndex);
        }
        return;
    }

    if (typeIndex == 3) { // Image File
        title = tr("Select Image File");
        filter = tr("Image Files (*.jpg *.jpeg *.png *.bmp *.tif)");
    } else { // Video File (Default)
        title = tr("Select Video File");
        filter = tr("Video Files (*.mp4 *.avi *.mkv *.mov)");
    }
    
    QString fileName = QFileDialog::getOpenFileName(this, title, "", filter);
    if (!fileName.isEmpty()) {
        ui->lineEdit_source->blockSignals(true); 
        ui->lineEdit_source->setText(fileName);
        ui->lineEdit_source->blockSignals(false);
        m_inputDebounceTimer->stop();

        m_channelSettings[lockedIndex].sourcePath = fileName;
        stopChannel(lockedIndex);
        startChannel(lockedIndex);
    }
}

void MainWindow::on_comboBox_sourceType_currentIndexChanged(int index)
{
    // Index 0: Video File -> 显示浏览按钮
    // Index 1: RTSP Stream -> 隐藏浏览按钮
    // Index 2: USB Camera -> 隐藏浏览按钮 (通常用索引 0, 1)
    // Index 3: Image File -> 显示浏览按钮
    
    // 保存当前设置到结构体
    m_channelSettings[m_currentChannelIndex].sourceTypeIndex = index;
    
    // 切换类型时清空输入框，避免残留上一种类型的路径
    ui->lineEdit_source->clear();
    
    bool isFile = (index == 0 || index == 3); // Video or Image needs browse button
    bool isCamera = (index == 2); // 摄像头也显示浏览按钮
    ui->btn_browse->setVisible(isFile || isCamera);
    ui->lineEdit_source->setPlaceholderText(isFile ? "File path..." : (isCamera ? "Click Browse to select camera..." : "rtsp://192.168.1.x:554/stream"));
}

void MainWindow::on_comboBox_channelSelect_currentIndexChanged(int index)
{
    if (index < 0) return;
    m_currentChannelIndex = index;
    loadChannelSettings(index);
    
    // 更新所有视频控件的高亮状态
    forEachVideoWidget([index](VideoWidget* v) {
        v->setSelected(v->property("channelIndex").toInt() == index);
    });
    
    ui->statusbar->showMessage(QString("Viewing configuration for CAM %1").arg(index + 1), 3000);
}

void MainWindow::on_lineEdit_source_textChanged(const QString &arg1)
{
    int targetIndex = m_currentChannelIndex;
    // 如果路径没变（比如切换频道加载旧设置），则不触发重启
    if (m_channelSettings[targetIndex].sourcePath == arg1 && m_decoders.contains(targetIndex)) {
        return;
    }

    m_channelSettings[targetIndex].sourcePath = arg1;

    // 使用成员定时器实现真正的防抖，并锁定 targetIndex
    m_inputDebounceTimer->stop();
    m_inputDebounceTimer->disconnect();
    connect(m_inputDebounceTimer, &QTimer::timeout, this, [this, targetIndex]() {
        if (m_channelSettings[targetIndex].sourcePath.isEmpty()) return;
        this->stopChannel(targetIndex);
        this->startChannel(targetIndex);
    });
    m_inputDebounceTimer->start(800);
}

void MainWindow::loadChannelSettings(int channelIndex)
{
    // 从 Map 中获取设置，如果没有则使用默认值
    ChannelSettings settings = m_channelSettings.value(channelIndex);
    
    // 更新 UI 控件，注意要暂时屏蔽信号防止循环触发保存逻辑（虽然这里逻辑简单可能不需要，但好习惯）
    ui->lineEdit_source->blockSignals(true); // 防止触发 textChanged -> stop/start
    ui->comboBox_sourceType->setCurrentIndex(settings.sourceTypeIndex);
    ui->lineEdit_source->setText(settings.sourcePath);
    ui->lineEdit_source->blockSignals(false);

    // 载入类别名称文件（只做一次）
    if (m_classNames.empty()) {
        QFile f(m_classesPath);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&f);
            while (!in.atEnd()) {
                QString line = in.readLine().trimmed();
                if (!line.isEmpty()) m_classNames.push_back(line);
            }
            f.close();
        }
    }
}

void MainWindow::enterAnalysisMode(int channelIndex)
{
    // 1. 获取当前通道配置
    ChannelSettings settings = m_channelSettings.value(channelIndex);
    const bool hasSource = !settings.sourcePath.trimmed().isEmpty();
    const bool hasActiveDecoder = m_decoders.contains(channelIndex);

    if (!hasSource || !hasActiveDecoder) {
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Warning);
        msg.setWindowTitle("Cannot Enter Analysis");
        msg.setText("当前通道没有可用播放源，请先配置并启动该通道后再进入详情模式。");
        msg.setStandardButtons(QMessageBox::Ok);
        msg.setStyleSheet(
            "QDialog {"
            " background: #eef4f1;"
            "}"
            "QLabel {"
            " color: #324654;"
            " font-size: 12px;"
            "}"
            "QPushButton {"
            " background-color: #7faea0;"
            " color: #ffffff;"
            " border: 1px solid #6d998c;"
            " border-radius: 6px;"
            " padding: 6px 14px;"
            " font-weight: 700;"
            "}"
            "QPushButton:hover {"
            " background-color: #6f9f91;"
            " border: 1px solid #608a7e;"
            "}");
        msg.exec();
        return;
    }

    bool isFile = (settings.sourceTypeIndex == 0); // 0: File
    bool isImage = (settings.sourceTypeIndex == 3); // 3: Image

    m_isAnalysisMode = true; 

    // 停止网格模式所有 VideoWidget 的刷新
    forEachVideoWidget([](VideoWidget* v) {
        v->setFrozen(true);
    });

    // 详情模式下：解码器继续运行用于检测
    for (auto it = m_decoders.begin(); it != m_decoders.end(); ++it) {
        int id = it.key();
        VideoDecoder* d = it.value();
        DisplayWorker* w = m_displayManager->getWorker(id);
        
        // 所有通道保持解码，但停止渲染到网格（省 GPU 资源）
        d->setLowFPSMode(true);
        d->setTargetFPS(30);
        
        // 对于当前分析的直播通道，保持渲染（详情模式需要用）
        // 对于文件源，详情模式用独立解码器，可以关闭网格的渲染
        if (w) {
            bool isCurrentLiveChannel = (id == channelIndex && !isFile && !isImage);
            w->setRenderingEnabled(isCurrentLiveChannel);
        }
    }

    // 隐藏右侧的设置面板，专注于分析
    ui->widget_sidebar_top->setVisible(false);
    ui->widget_sidebar_analysis->setVisible(true); // 显示分析模式的侧边栏
    
    // 切换到 "Detections" 标签页，显示详细信息表格
    ui->tabWidget_details->setCurrentWidget(ui->tab_detections);

    // 重置并显示暂停按钮
    ui->btn_pause->setChecked(false);
    
    // 图片模式下直接隐藏整个控制组，避免显示空的 "Video Controls" 框
    ui->groupBox_video_controls->setVisible(!isImage);

    if (isImage) {
        // 图片模式：无需额外操作，因为父容器已隐藏
    } else if (isFile) {
        ui->btn_pause->setVisible(true);
        ui->btn_pause->setText("Pause");
        // 文件模式：显示进度条和倍速，隐藏 Live 标志
        ui->label_seek->setVisible(true);
        ui->slider_seek->setVisible(true);
        ui->label_speed->setVisible(true);
        ui->comboBox_speed->setVisible(true);
        ui->label_live_indicator->setVisible(false);
        
        // 重置倍速为 1x
        ui->comboBox_speed->blockSignals(true);
        ui->comboBox_speed->setCurrentIndex(1); // 假设 1.0x 是第二个选项
        ui->comboBox_speed->blockSignals(false);
        if (m_decoders.contains(channelIndex)) {
            m_decoders[channelIndex]->setSpeed(1.0f);
        }

        // 如果检测已开启，进入详情时立即确保该通道检测流在运行并从头同步
        if (m_detectionEnabled && m_decoders.contains(channelIndex)) {
            m_decoders[channelIndex]->seek(0);
            m_decoders[channelIndex]->setPaused(false);
            DetectionResults::getInstance().clear(channelIndex);
            ChannelResultQueue::getInstance().clearChannel(channelIndex);
            ui->tableWidget_detections->setRowCount(0);
        }
        
        // 初始化进度条范围（基于视频时长）
        if (m_decoders.contains(channelIndex)) {
            int64_t duration = m_decoders[channelIndex]->getDuration();
            if (duration > 0) {
                ui->slider_seek->setRange(0, 1000); // 使用千分比
                ui->slider_seek->setValue(0);
            }
        }
        // 启动进度更新定时器
        m_progressUpdateTimer->start(500);
    } else {
        ui->btn_pause->setVisible(true);
        ui->btn_pause->setText("Freeze");
        // 直播模式：隐藏进度条和倍速，显示 Live 标志
        ui->label_seek->setVisible(false);
        ui->slider_seek->setVisible(false);
        ui->label_speed->setVisible(false);
        ui->comboBox_speed->setVisible(false);
        ui->label_live_indicator->setVisible(true);
    }

    // 3. 准备大屏播放控件
    // 清空旧的（如果有）
    QLayoutItem *child;
    while ((child = ui->layout_analysis_video_placeholder->takeAt(0)) != 0) {
        if (child->widget()) delete child->widget();
        delete child;
    }
    
    // 清理之前的独立解码器
    cleanupAnalysisDecoder();

    // 创建新的大屏控件
    VideoWidget *bigVideo = new VideoWidget(this);
    bigVideo->setChannelId(channelIndex);
    bigVideo->setText(QString("CAM %1 - ANALYSIS MODE").arg(channelIndex + 1));
    
    if (isFile && !settings.sourcePath.isEmpty()) {
        // 视频文件：创建独立的解码器，不影响网格模式的检测
        m_analysisFrameQueue = new FrameQueue(5);
        m_analysisDecoder = new VideoDecoder(settings.sourcePath.toStdString(), -1, m_analysisFrameQueue);
        
        // 创建 DisplayWorker 并移动到独立线程
        m_analysisWorker = new DisplayWorker(-1, m_analysisFrameQueue);
        m_analysisDisplayThread = new QThread(this);
        m_analysisWorker->moveToThread(m_analysisDisplayThread);
        connect(m_analysisDisplayThread, &QThread::started, m_analysisWorker, &DisplayWorker::processLoop);
        connect(m_analysisWorker, &DisplayWorker::finished, m_analysisDisplayThread, &QThread::quit);
        
        // 在新线程中启动解码器
        m_analysisThread = new QThread(this);
        m_analysisDecoder->moveToThread(m_analysisThread);
        connect(m_analysisThread, &QThread::started, m_analysisDecoder, &VideoDecoder::startDecoding);
        connect(m_analysisDecoder, &VideoDecoder::playbackFinished, this, &MainWindow::onPlaybackFinished, Qt::QueuedConnection);
        
        // 启动两个线程：解码线程和显示线程
        m_analysisDisplayThread->start();
        m_analysisThread->start();
        
        // 绑定到独立解码器的数据源
        bigVideo->setDataSource(&m_analysisWorker->current_ptr, &m_analysisWorker->current_w,
                    &m_analysisWorker->current_h, &m_analysisWorker->current_pitch,
                    &m_analysisWorker->current_format);
        bigVideo->setRefreshFPS(60);
    } else {
        // 直播/摄像头：共享网格模式的数据源
        DisplayWorker* worker = m_displayManager->getWorker(channelIndex);
        if (worker) {
            bigVideo->setDataSource(&worker->current_ptr, &worker->current_w,
                                    &worker->current_h, &worker->current_pitch,
                                    &worker->current_format);
            bigVideo->setRefreshFPS(60);
        }
    }
    
    // 详情模式下隐藏关闭按钮（必须在 setDataSource 之后调用，因为 setDataSource 会 show 按钮）
    bigVideo->setCloseBtnVisible(false);

    // 详情模式下，强制请求全高清分辨率
    // 这样可以看清细节，且此时只有一路视频，带宽不是问题
    bigVideo->setProperty("targetWidth", 1920);
    bigVideo->setProperty("targetHeight", 1080);
    
    // 将大屏控件加入布局
    ui->layout_analysis_video_placeholder->addWidget(bigVideo);

    // 4. 更新标题并切换页面
    ui->groupBox_monitor->setTitle(QString("Analysis Mode - CAM %1").arg(channelIndex + 1));
    ui->stackedWidget_view->setCurrentIndex(1); // 切换到 Analysis Page
    
    // 视频文件：详情模式使用独立解码器，不需要检查网格解码器的状态
    // 独立解码器会从头开始播放，按钮应该是正常的 Pause 状态

    // 5. 同步选中状态
    m_currentChannelIndex = channelIndex;
    ui->comboBox_channelSelect->setCurrentIndex(channelIndex);

    // 详情模式立即刷新一次检测信息栏，并确保定时刷新处于开启状态
    if (m_detectionUpdateTimer && !m_detectionUpdateTimer->isActive()) {
        m_detectionUpdateTimer->start();
    }
    {
        auto dets = DetectionResults::getInstance().get(channelIndex);
        ui->tableWidget_detections->setRowCount(0);
        for (const auto &d : dets) {
            int row = ui->tableWidget_detections->rowCount();
            ui->tableWidget_detections->insertRow(row);
            QString name = QString::number(d.class_id);
            if (static_cast<size_t>(d.class_id) < m_classNames.size()) name = m_classNames[d.class_id];
            ui->tableWidget_detections->setItem(row, 0, new QTableWidgetItem(name));
            ui->tableWidget_detections->setItem(row, 1, new QTableWidgetItem(QString::number(d.conf, 'f', 2)));
            ui->tableWidget_detections->setItem(row, 2, new QTableWidgetItem(QString("[%1, %2, %3, %4]")
                .arg(d.x).arg(d.y).arg(d.w).arg(d.h)));
        }
    }

    logSystemMessage(QString("Entered Analysis Mode for CAM %1").arg(channelIndex + 1));
}

void MainWindow::exitAnalysisMode()
{
    ui->stackedWidget_view->setUpdatesEnabled(false);

    // 恢复右侧设置面板
    ui->widget_sidebar_top->setVisible(true);
    ui->widget_sidebar_analysis->setVisible(false);

    m_isAnalysisMode = false; 
    
    // 停止进度更新定时器
    if (m_progressUpdateTimer) m_progressUpdateTimer->stop();
    
    // 清理详情模式的独立解码器
    cleanupAnalysisDecoder();
    
    // 退出详情模式，恢复网格模式显示
    for (auto it = m_decoders.begin(); it != m_decoders.end(); ++it) {
        it.value()->setLowFPSMode(true);
        it.value()->setTargetFPS(30);
        DisplayWorker* w = m_displayManager->getWorker(it.key());
        if (w) w->setRenderingEnabled(true);
    }

    // 切换回网格页面
    ui->stackedWidget_view->setCurrentIndex(0);
    ui->groupBox_video_controls->setVisible(false);

    // 恢复标题
    int index = ui->comboBox_layout->currentIndex();
    QString title = "Real-time Monitoring";
    switch (index) {
        case 0: title += " (1-CH)"; break;
        case 1: title += " (4-CH)"; break;
        case 2: title += " (9-CH)"; break;
    }
    ui->groupBox_monitor->setTitle(title);

    // 延迟到页面切换完成后再恢复网格刷新，降低返回时抖动感
    QTimer::singleShot(0, this, [this]() {
        forEachVideoWidget([](VideoWidget* v) {
            v->setFrozen(false);
            v->update();
        });
        ui->stackedWidget_view->setUpdatesEnabled(true);
    });

    logSystemMessage("Returned to Grid View.");
}

void MainWindow::on_btn_back_to_grid_clicked()
{
    exitAnalysisMode();
}

void MainWindow::on_btn_pause_toggled(bool checked)
{
    // 获取当前通道配置以区分文件还是直播
    ChannelSettings settings = m_channelSettings.value(m_currentChannelIndex);
    bool isFile = (settings.sourceTypeIndex == 0);
    bool isImage = (settings.sourceTypeIndex == 3);

    if (isImage) return; // 图片模式下不应触发此逻辑

    // Freeze 仅冻结画面，不影响后台检测

    // 检测是否是"重新播放"状态（按钮文字为 Replay）
    bool isReplayState = (ui->btn_pause->text() == "Replay");
    
    if (isReplayState && !checked) {
        // 用户点击"重新播放"，需要跳转到开头并恢复播放
        // 使用独立解码器
        if (m_analysisDecoder) {
            m_analysisDecoder->seek(0);
            m_analysisDecoder->setPaused(false);
        }

        // 同步重启后台检测源，避免详情回放与检测结果脱节
        if (m_decoders.contains(m_currentChannelIndex)) {
            m_decoders[m_currentChannelIndex]->seek(0);
            m_decoders[m_currentChannelIndex]->setPaused(false);
        }
        DetectionResults::getInstance().clear(m_currentChannelIndex);
        ChannelResultQueue::getInstance().clearChannel(m_currentChannelIndex);
        ui->tableWidget_detections->setRowCount(0);
        if (m_detectionUpdateTimer && !m_detectionUpdateTimer->isActive()) {
            m_detectionUpdateTimer->start();
        }

        // 重置按钮外观
        ui->btn_pause->setText(isFile ? "Pause" : "Freeze");
        ui->btn_pause->setIcon(QIcon::fromTheme("media-playback-pause"));
        ui->btn_pause->setStyleSheet("");
        ui->slider_seek->setValue(0);
        logSystemMessage(QString("CAM %1 replaying from start.").arg(m_currentChannelIndex + 1));
        return;
    }

    // 1. 更新按钮外观
    if (checked) {
        ui->btn_pause->setText(isFile ? "Play" : "Resume");
        ui->btn_pause->setIcon(QIcon::fromTheme("media-playback-start"));
        ui->btn_pause->setStyleSheet("background-color: #e65100; color: white;"); // 橙色高亮
        logSystemMessage(QString("CAM %1 %2").arg(m_currentChannelIndex + 1).arg(isFile ? "Playback Paused." : "Stream Frozen."), 1);
        // Freeze 时停止检测信息更新，避免 UI 显示动态变化
        m_detectionUpdateTimer->stop();
    } else {
        ui->btn_pause->setText(isFile ? "Pause" : "Freeze");
        ui->btn_pause->setIcon(QIcon::fromTheme("media-playback-pause"));
        ui->btn_pause->setStyleSheet(""); // 恢复默认
        logSystemMessage(QString("CAM %1 %2").arg(m_currentChannelIndex + 1).arg(isFile ? "Playback Resumed." : "Stream Resumed."), 0);
        // 恢复时重新启动检测信息更新
        m_detectionUpdateTimer->start();
    }

    // 2. 区分文件暂停和直播/摄像头 freeze
    if (isFile) {
        // 文件模式：暂停独立解码器（不影响网格模式的解码器）
        if (m_analysisDecoder) {
            m_analysisDecoder->setPaused(checked);
        }
        // 文件详情模式下，同步暂停后台检测通道，保证框与信息一致
        if (m_decoders.contains(m_currentChannelIndex)) {
            m_decoders[m_currentChannelIndex]->setPaused(checked);
        }
    } else {
        // 直播/摄像头模式：只 freeze 显示，后台继续解码
        // 找到详情模式的大屏 VideoWidget
        if (ui->layout_analysis_video_placeholder->count() > 0) {
            QWidget* w = ui->layout_analysis_video_placeholder->itemAt(0)->widget();
            VideoWidget* bigVideo = qobject_cast<VideoWidget*>(w);
            if (bigVideo) {
                bigVideo->setFrozen(checked);
            }
        }
    }
}

void MainWindow::on_comboBox_speed_currentIndexChanged(int index)
{
    // 解析速度选项 (例如 "0.5x", "1.0x", "2.0x")
    QString text = ui->comboBox_speed->itemText(index);
    bool ok = false;
    float speed = 1.0f;
    if (text.endsWith('x')) {
        speed = text.left(text.size() - 1).toFloat(&ok);
    }
    if (!ok) speed = 1.0f;

    // 更新独立解码器的播放速率（不影响网格模式的解码器）
    if (m_analysisDecoder) {
        m_analysisDecoder->setSpeed(speed);
    }
    // 同步后台检测通道播放速率，避免详情画面与检测节奏不一致
    if (m_decoders.contains(m_currentChannelIndex)) {
        m_decoders[m_currentChannelIndex]->setSpeed(speed);
    }

    // 如果处于详情模式，调整大屏的刷新频率 (默认 60 * speed)
    if (m_isAnalysisMode) {
        if (ui->layout_analysis_video_placeholder->count() > 0) {
            QWidget* w = ui->layout_analysis_video_placeholder->itemAt(0)->widget();
            VideoWidget* bigVideo = qobject_cast<VideoWidget*>(w);
            if (bigVideo) bigVideo->setRefreshFPS(static_cast<int>(60 * speed));
        }
    }
    
    logSystemMessage(QString("Playback speed set to %1x").arg(speed, 0, 'f', 1));
}

void MainWindow::on_slider_seek_sliderReleased()
{
    performSeekFromSlider();
}

void MainWindow::performSeekFromSlider()
{
    // 使用独立解码器
    if (!m_analysisDecoder) return;
    
    int64_t duration = m_analysisDecoder->getDuration();
    if (duration <= 0) return; // 直播流无法跳转
    
    // 计算目标时间戳（毫秒）
    int sliderValue = ui->slider_seek->value(); // 0-1000
    int64_t targetMs = duration * sliderValue / 1000;
    
    // 执行跳转
    m_analysisDecoder->seek(targetMs);

    // 详情模式拖动进度条时，同步后台检测解码器位置
    if (m_decoders.contains(m_currentChannelIndex)) {
        VideoDecoder* d = m_decoders[m_currentChannelIndex];
        d->seek(targetMs);
        d->setPaused(false);
        DetectionResults::getInstance().clear(m_currentChannelIndex);
        ChannelResultQueue::getInstance().clearChannel(m_currentChannelIndex);
        ui->tableWidget_detections->setRowCount(0);
    }
    
    // 如果处于 Replay 状态，重置按钮为正常状态
    if (ui->btn_pause->text() == "Replay") {
        ChannelSettings settings = m_channelSettings.value(m_currentChannelIndex);
        bool isFile = (settings.sourceTypeIndex == 0);
        
        ui->btn_pause->blockSignals(true);
        ui->btn_pause->setChecked(false);
        ui->btn_pause->setText(isFile ? "Pause" : "Freeze");
        ui->btn_pause->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
        ui->btn_pause->setStyleSheet("");
        ui->btn_pause->blockSignals(false);
        
        // 确保恢复播放
        m_analysisDecoder->setPaused(false);
    }
    
    logSystemMessage(QString("Seek to %1:%2").arg(targetMs / 60000).arg((targetMs / 1000) % 60, 2, 10, QChar('0')));
}

void MainWindow::onPlaybackFinished(int channel_id)
{
    // 守卫：如果通道已被 stopChannel 销毁（decoder 不存在），
    // 说明这是 QueuedConnection 延迟到达的旧信号，直接忽略。
    // 典型场景：压测结束后 m_loadTestRealDecode 已置 false，
    // 延迟信号落到下方 showPlaybackFinished(true) 导致 replay 按钮残留。
    if (channel_id >= 0 && !m_decoders.contains(channel_id)) {
        return;
    }

    // 压测真实解码模式：自动循环播放
    if (m_loadTestRealDecode && m_loadTestChannels.contains(channel_id)) {
        if (m_decoders.contains(channel_id)) {
            VideoDecoder* d = m_decoders[channel_id];
            d->seek(0);
            d->setPaused(false);
        }
        return;
    }

    // 区分独立解码器和网格模式解码器
    if (channel_id == -1) {
        // 详情模式：独立解码器播放结束
        if (m_isAnalysisMode) {
            ui->btn_pause->blockSignals(true);
            ui->btn_pause->setChecked(true);
            ui->btn_pause->setText("Replay");
            ui->btn_pause->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
            ui->btn_pause->setStyleSheet(
                "QPushButton {"
                " background-color: #7faea0;"
                " color: white;"
                " border: 1px solid #6d998c;"
                "}"
                "QPushButton:hover {"
                " background-color: #6f9f91;"
                " border: 1px solid #608a7e;"
                "}");
            ui->btn_pause->blockSignals(false);
            ui->slider_seek->setValue(1000);
            logSystemMessage(QString("CAM %1 playback finished.").arg(m_currentChannelIndex + 1));
        }
        return;
    }
    
    // 网格模式解码器播放结束：显示重播按钮
    if (VideoWidget* v = findVideoWidget(channel_id)) {
        v->showPlaybackFinished(true);
    }
    
    logSystemMessage(QString("CAM %1 playback finished.").arg(channel_id + 1));
}

void MainWindow::onVideoReplayRequested(int channel_id)
{
    if (!m_decoders.contains(channel_id)) return;
    
    VideoDecoder* d = m_decoders[channel_id];
    d->seek(0);
    d->setPaused(false);
    
    // 隐藏重播按钮（已在点击时隐藏，这里可省略）
    
    logSystemMessage(QString("CAM %1 replaying from start.").arg(channel_id + 1));
}

void MainWindow::on_btn_snapshot_clicked()
{
    // 1. 找到当前详情模式下的大屏控件
    // layout_analysis_video_placeholder 里面只有一个 VideoWidget
    QLayoutItem *item = ui->layout_analysis_video_placeholder->itemAt(0);
    if (!item || !item->widget()) {
        logSystemMessage("Snapshot failed: No video source found.", 2);
        return;
    }

    QWidget *videoWidget = item->widget();

    // 2. 抓取当前画面 (包含绘制的检测框)
    QPixmap pixmap = videoWidget->grab();

    // 3. 准备保存路径
    QString dirPath = QCoreApplication::applicationDirPath() + "/snapshots";
    QDir dir(dirPath);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz");
    QString fileName = QString("CAM%1_%2.png").arg(m_currentChannelIndex + 1).arg(timestamp);
    QString fullPath = dir.filePath(fileName);

    // 4. 保存文件
    if (pixmap.save(fullPath)) {
        logSystemMessage(QString("Snapshot saved: %1").arg(fileName), 0);
    } else {
        logSystemMessage("Snapshot failed: Could not write file.", 2);
    }
}

void MainWindow::on_btn_start_clicked()
{
    // 开启检测：加载模型、启动 worker、启用输入/推理资源池
    startDetection();

    // 更新 UI 状态：防止重复点击 Start
    ui->btn_start->setEnabled(false);
    ui->btn_stop->setEnabled(true);
    ui->btn_stop->setText("STOP");
}

void MainWindow::on_btn_stop_clicked()
{
    // 如果检测仍在进行，则先停止检测并保留结果，按钮变为生成报告
    if (m_detectionEnabled) {
        stopDetection(false);
        // 清除当前帧的检测框显示（保留累计统计用于报告）
        // 只清除检测结果，不清除视频显示——视频源继续正常播放
        DetectionResults::getInstance().clearAll();
        ChannelResultQueue::getInstance().clearAll();
        ui->tableWidget_detections->setRowCount(0);
        // 触发一次重绘以刷掉残留的检测框
        forEachVideoWidget([](VideoWidget* v){ v->update(); });
        ui->btn_stop->setText("Generate Report");
        ui->btn_start->setEnabled(true); // 允许重新开始
        // 保持 btn_stop 可用以便生成报告
    } else {
        // 如果检测已经停止，点击则触发生成报告流程
        generateReport();
    }
}

void MainWindow::logSystemMessage(const QString &msg, int level)
{
    QString color = "#cccccc"; // Default Info
    if (level == 1) color = "#ffaa00"; // Warning (Orange)
    if (level == 2) color = "#ff4444"; // Error (Red)

    QString timestamp = QDateTime::currentDateTime().toString("HH:mm:ss");
    QString html = QString("<span style='color:#666'>[%1]</span> <span style='color:%2'>%3</span>")
                           .arg(timestamp, color, msg);
    
    
    // 同时在窗体底部的状态栏显示最新一条消息
    ui->statusbar->showMessage(msg, 3000); // 显示 3 秒
}

void MainWindow::startDetection()
{
    if (m_detectionEnabled) return;

    // 1. 加载模型（只加载一次）
    if (!m_modelLoaded) {
        if (!TRTDetector::getInstance().load(m_modelPath.toStdString())) {
            logSystemMessage("Failed to load model: " + m_modelPath, 2);
            return;
        }
        m_modelLoaded = true;
    }

    // 2. 初始化推理资源池（每次检测启动初始化）
    if (!m_memoryInited) {
        int model_w = TRTDetector::getInstance().getInputW();
        int model_h = TRTDetector::getInstance().getInputH();
        int effectiveBatch = std::min(m_workerMaxBatch, TRTDetector::getInstance().getMaxBatch());
        int input_frames = std::max(16, m_inputArenaFrames);
        size_t desiredSlots = static_cast<size_t>(std::clamp((input_frames + std::max(1, effectiveBatch) - 1) / std::max(1, effectiveBatch) * 2,
                                     16, 256));
        size_t input_bytes_per_slot = static_cast<size_t>(effectiveBatch) * 3 * model_w * model_h * sizeof(float);
        size_t output_bytes_per_slot = TRTDetector::getInstance().getOutputBytesPerBatch() * static_cast<size_t>(effectiveBatch);
        size_t input_arena_bytes = desiredSlots * input_bytes_per_slot;
        size_t output_arena_bytes = desiredSlots * output_bytes_per_slot;

        if (!SlotPool::getInstance().init(desiredSlots, effectiveBatch)) {
            logSystemMessage("Failed to init SlotPool.", 2);
            return;
        }
        if (!TensorArenaManager::getInstance().init(input_arena_bytes, output_arena_bytes)) {
            logSystemMessage("Failed to init TensorArenaManager.", 2);
            return;
        }
        m_memoryInited = true;
        logSystemMessage(QString("Tensor arenas ready: slots=%1, inputFrames=%2, effectiveBatch=%3 (worker=%4, model=%5)")
            .arg(static_cast<int>(desiredSlots)).arg(input_frames).arg(effectiveBatch).arg(m_workerMaxBatch).arg(TRTDetector::getInstance().getMaxBatch()), 0);
    }

    int effectiveBatch = std::min(m_workerMaxBatch, TRTDetector::getInstance().getMaxBatch());

    // 4. 初始化输入帧 Arena（按任务书：默认 200 帧）
    {
        int model_w = TRTDetector::getInstance().getInputW();
        int model_h = TRTDetector::getInstance().getInputH();
        size_t single_frame_nv12 = static_cast<size_t>(model_w) * model_h * 3 / 2;
        int input_frames = std::max(16, m_inputArenaFrames);
        size_t input_arena_bytes = static_cast<size_t>(input_frames) * single_frame_nv12;
        size_t max_ready_frames = static_cast<size_t>(input_frames);
        if (!InputFrameArenaStore::getInstance().init(input_arena_bytes, single_frame_nv12,
                                                      static_cast<size_t>(model_w), max_ready_frames)) {
            logSystemMessage("Failed to init InputFrameArenaStore.", 2);
            return;
        }
        InputFrameArenaStore::getInstance().enable();
    }

    // 重置累计统计（新一轮检测）
    ChannelResultQueue::getInstance().clearAll();
    DetectionResults::getInstance().resetAccumulated();
    PipelineStats::getInstance().resetAll();
    m_detectionStartTime = QDateTime::currentDateTime();
    m_sourceHistory.clear(); // 重置源切换历史

    // 启动性能监控定时器
    if (!m_pipelineStatsTimer) {
        m_pipelineStatsTimer = new QTimer(this);
        // connect(m_pipelineStatsTimer, &QTimer::timeout, this, &MainWindow::printPipelineStats);
    }
    {
        auto diskSettings = AdvancedSettingsDialog::loadFromDisk();
        m_pipelineStatsTimer->start(diskSettings.statsInterval * 1000);
    }

    // 5. 启动 Worker
    // 新策略：并行度参数即 Worker 数，每个 Worker 固定绑定一个 context + 一个 stream。
    int desiredWorkers = std::max(1, m_inferenceStreams);
    if (m_workerCount != desiredWorkers) {
        for (auto &w : m_workers) w->stop();
        m_workers.clear();
        m_workerCount = desiredWorkers;
    }
    if (m_workers.empty()) {
        for (int i = 0; i < m_workerCount; ++i) {
            auto w = std::make_unique<Worker>(
                i,
                static_cast<size_t>(m_workerMaxBatch),
                std::chrono::milliseconds(5));
            w->start();
            m_workers.push_back(std::move(w));
        }
    }

    m_detectionEnabled = true;
    logSystemMessage(QString("Detection started. parallelWorkers=%1, batch=%2")
        .arg(desiredWorkers).arg(m_workerMaxBatch), 0);

    // 图片源在启动检测后可能不再有新帧，重新启动一次通道以推入检测队列
    for (auto it = m_channelSettings.begin(); it != m_channelSettings.end(); ++it) {
        int ch = it.key();
        const ChannelSettings &s = it.value();
        if (s.sourceTypeIndex == 3 && !s.sourcePath.isEmpty()) {
            if (m_decoders.contains(ch)) {
                stopChannel(ch);
                startChannel(ch);
            }
        }
    }
}

void MainWindow::stopDetection(bool clear_results)
{
    if (!m_detectionEnabled && m_workers.empty()) {
        if (clear_results) {
            DetectionResults::getInstance().clearAll();
            ChannelResultQueue::getInstance().clearAll();
        }
        InputFrameArenaStore::getInstance().disable();
        InputFrameArenaStore::getInstance().shutdown();
        TensorArenaManager::getInstance().shutdown();
        SlotPool::getInstance().shutdown();
        m_memoryInited = false;
        return;
    }

    m_detectionEnabled = false;
    ChannelResultQueue::getInstance().clearAll();
    InputFrameArenaStore::getInstance().disable();
    InputFrameArenaStore::getInstance().shutdown();

    // 停止性能监控定时器
    if (m_pipelineStatsTimer) m_pipelineStatsTimer->stop();

    for (auto &w : m_workers) w->stop();
    m_workers.clear();
    TensorArenaManager::getInstance().shutdown();
    SlotPool::getInstance().shutdown();
    m_memoryInited = false;

    if (clear_results) {
        DetectionResults::getInstance().clearAll();
        ChannelResultQueue::getInstance().clearAll();
    }
    logSystemMessage("Detection stopped.", 1);
}

void MainWindow::generateReport()
{
    auto accumulated = DetectionResults::getInstance().getAccumulated();
    if (accumulated.empty()) {
        logSystemMessage("No detection data to generate report.", 1);
        return;
    }

    QDateTime now = QDateTime::currentDateTime();
    QString duration = "N/A";
    if (m_detectionStartTime.isValid()) {
        qint64 secs = m_detectionStartTime.secsTo(now);
        int h = static_cast<int>(secs / 3600);
        int m = static_cast<int>((secs % 3600) / 60);
        int s = static_cast<int>(secs % 60);
        duration = QString("%1h %2m %3s").arg(h).arg(m).arg(s);
    }

    // 汇总统计
    int totalFrames = 0;
    int totalDetections = 0;
    std::unordered_map<int, int> globalClassCounts;
    std::unordered_map<int, float> globalClassMaxConf;

    // 按通道 ID 排序，确保报告中通道顺序为 1,2,3,...
    std::vector<int> sortedChannelIds;
    for (const auto& [ch_id, stats] : accumulated) {
        sortedChannelIds.push_back(ch_id);
    }
    std::sort(sortedChannelIds.begin(), sortedChannelIds.end());

    for (int ch_id : sortedChannelIds) {
        const auto& stats = accumulated[ch_id];
        totalFrames += stats.total_frames;
        for (const auto& [cls_id, cls_stats] : stats.class_stats) {
            totalDetections += cls_stats.count;
            globalClassCounts[cls_id] += cls_stats.count;
            globalClassMaxConf[cls_id] = std::max(globalClassMaxConf[cls_id], cls_stats.max_conf);
        }
    }

    // 通用 HTML 生成 lambda（参数化中英文）
    struct ReportLabels {
        QString title, reportTime, detDuration, model, channelsAnalyzed;
        QString totalInfFrames, totalDet, uniqueClasses;
        QString overallClassSummary, cls, totalCount, channel;
        QString source, infFrames, detections, maxConf, noDetections, footer;
        QString sourceHistory; // 源切换历史标签
    };

    auto buildHtml = [&](const ReportLabels& L) -> QString {
        QString html;
        html += "<!DOCTYPE html>\n<html><head><meta charset='utf-8'>\n";
        html += QString("<title>%1</title>\n").arg(L.title);
        html += "<style>\n";
        html += "body { font-family: 'Segoe UI', 'Microsoft YaHei', Arial, sans-serif; margin: 40px; background: #f5f5f5; }\n";
        html += ".container { max-width: 900px; margin: 0 auto; background: #fff; padding: 30px; border-radius: 8px; box-shadow: 0 2px 8px rgba(0,0,0,0.1); }\n";
        html += "h1 { color: #333; border-bottom: 2px solid #4a90d9; padding-bottom: 10px; }\n";
        html += "h2 { color: #4a90d9; margin-top: 30px; }\n";
        html += ".meta { color: #666; margin-bottom: 20px; }\n";
        html += "table { width: 100%; border-collapse: collapse; margin: 15px 0; }\n";
        html += "th { background: #4a90d9; color: #fff; padding: 10px 12px; text-align: left; }\n";
        html += "td { padding: 8px 12px; border-bottom: 1px solid #eee; }\n";
        html += "tr:hover td { background: #f0f7ff; }\n";
        html += ".summary-box { background: #e8f4fd; padding: 15px; border-radius: 6px; margin: 15px 0; }\n";
        html += ".summary-box span { font-weight: bold; color: #333; }\n";
        html += "</style>\n</head><body>\n<div class='container'>\n";

        html += QString("<h1>🔍 %1</h1>\n").arg(L.title);
        html += "<div class='meta'>\n";
        html += QString("<p><b>%1:</b> %2</p>\n").arg(L.reportTime, now.toString("yyyy-MM-dd HH:mm:ss"));
        html += QString("<p><b>%1:</b> %2</p>\n").arg(L.detDuration, duration);
        html += QString("<p><b>%1:</b> %2</p>\n").arg(L.model, m_modelPath);
        html += QString("<p><b>%1:</b> %2</p>\n").arg(L.channelsAnalyzed).arg(accumulated.size());
        html += "</div>\n";

        html += "<div class='summary-box'>\n";
        html += QString("<p><span>%1:</span> %2</p>\n").arg(L.totalInfFrames).arg(totalFrames);
        html += QString("<p><span>%1:</span> %2</p>\n").arg(L.totalDet).arg(totalDetections);
        html += QString("<p><span>%1:</span> %2</p>\n").arg(L.uniqueClasses).arg(globalClassCounts.size());
        html += "</div>\n";

        if (!globalClassCounts.empty()) {
            html += QString("<h2>%1</h2>\n").arg(L.overallClassSummary);
            html += QString("<table><tr><th>%1</th><th>%2</th></tr>\n").arg(L.cls, L.totalCount);
            for (const auto& [cls_id, count] : globalClassCounts) {
                QString name = QString::number(cls_id);
                if (static_cast<size_t>(cls_id) < m_classNames.size()) name = m_classNames[cls_id];
                html += QString("<tr><td>%1</td><td>%2</td></tr>\n").arg(name).arg(count);
            }
            html += "</table>\n";
        }

        // 按排序后的通道顺序输出
        for (int ch_id : sortedChannelIds) {
            const auto& stats = accumulated[ch_id];
            QString chSource = m_channelSettings.value(ch_id).sourcePath;
            if (chSource.isEmpty()) chSource = "N/A";

            html += QString("<h2>%1 %2</h2>\n").arg(L.channel).arg(ch_id + 1);
            html += QString("<p><b>%1:</b> %2</p>\n").arg(L.source, chSource.toHtmlEscaped());
            // 源切换历史
            if (m_sourceHistory.contains(ch_id) && m_sourceHistory[ch_id].size() > 1) {
                html += QString("<p><b>%1:</b></p>\n<ul>\n").arg(L.sourceHistory);
                for (const auto& entry : m_sourceHistory[ch_id]) {
                    html += QString("<li>%1</li>\n").arg(entry.toHtmlEscaped());
                }
                html += "</ul>\n";
            }
            html += QString("<p><b>%1:</b> %2</p>\n").arg(L.infFrames).arg(stats.total_frames);

            if (!stats.class_stats.empty()) {
                html += QString("<table><tr><th>%1</th><th>%2</th><th>%3</th></tr>\n")
                            .arg(L.cls, L.detections, L.maxConf);
                for (const auto& [cls_id, cls_stats] : stats.class_stats) {
                    QString name = QString::number(cls_id);
                    if (static_cast<size_t>(cls_id) < m_classNames.size()) name = m_classNames[cls_id];
                    html += QString("<tr><td>%1</td><td>%2</td><td>%3</td></tr>\n")
                                .arg(name)
                                .arg(cls_stats.count)
                                .arg(static_cast<double>(cls_stats.max_conf), 0, 'f', 3);
                }
                html += "</table>\n";
            } else {
                html += QString("<p><i>%1</i></p>\n").arg(L.noDetections);
            }
        }

        html += QString("<hr><p style='color:#999; font-size:12px;'>%1</p>\n").arg(L.footer);
        html += "</div>\n</body></html>\n";
        return html;
    };

    // 英文标签
    ReportLabels enLabels {
        "Detection Report", "Report Time", "Detection Duration", "Model", "Channels Analyzed",
        "Total Inference Frames", "Total Detections", "Unique Classes",
        "Overall Class Summary", "Class", "Total Count", "Channel",
        "Source", "Inference Frames", "Detections", "Max Confidence",
        "No detections in this channel.", "Generated by CudaForge-YOLO",
        "Source Change History"
    };

    // 中文标签
    ReportLabels zhLabels {
        QString::fromUtf8("检测报告"),
        QString::fromUtf8("报告时间"),
        QString::fromUtf8("检测时长"),
        QString::fromUtf8("模型"),
        QString::fromUtf8("分析通道数"),
        QString::fromUtf8("推理总帧数"),
        QString::fromUtf8("检测目标总数"),
        QString::fromUtf8("检测类别数"),
        QString::fromUtf8("全局类别统计"),
        QString::fromUtf8("类别"),
        QString::fromUtf8("总数"),
        QString::fromUtf8("通道"),
        QString::fromUtf8("视频源"),
        QString::fromUtf8("推理帧数"),
        QString::fromUtf8("检测数"),
        QString::fromUtf8("最大置信度"),
        QString::fromUtf8("该通道无检测结果。"),
        QString::fromUtf8("由 CudaForge-YOLO 生成"),
        QString::fromUtf8("源切换历史")
    };

    QString htmlEn = buildHtml(enLabels);
    QString htmlZh = buildHtml(zhLabels);

    // 保存到 reports 目录
    QString dirPath = QCoreApplication::applicationDirPath() + "/reports";
    QDir dir(dirPath);
    if (!dir.exists()) dir.mkpath(".");

    QString timestamp = now.toString("yyyyMMdd_HHmmss");
    bool savedAny = false;

    // 保存英文报告
    {
        QString fullPath = dir.filePath(QString("report_en_%1.html").arg(timestamp));
        QFile file(fullPath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << htmlEn;
            file.close();
            logSystemMessage(QString("EN Report saved: %1").arg(fullPath), 0);
            savedAny = true;
        }
    }
    // 保存中文报告
    {
        QString fullPath = dir.filePath(QString("report_zh_%1.html").arg(timestamp));
        QFile file(fullPath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << htmlZh;
            file.close();
            logSystemMessage(QString("ZH Report saved: %1").arg(fullPath), 0);
            savedAny = true;
        }
    }

    if (savedAny) {
        // 重置按钮状态
        ui->btn_stop->setEnabled(false);
        ui->btn_start->setEnabled(true);
        ui->btn_stop->setText("STOP");
    } else {
        logSystemMessage("Failed to write report files.", 2);
    }
}

void MainWindow::printPipelineStats()
{
    // 控制台统计已停用，避免与高级面板重复消费计数器导致 infer 显示异常
    return;

    auto& ps = PipelineStats::getInstance();

    // 读取并重置计数器（得到上个周期的增量）
    uint64_t decoded   = ps.frames_decoded.exchange(0, std::memory_order_relaxed);
    uint64_t pushed_dq = ps.frames_pushed_dq.exchange(0, std::memory_order_relaxed);
    uint64_t dropped_dq= ps.frames_dropped_dq.exchange(0, std::memory_order_relaxed);
    uint64_t batches   = ps.batches_inferred.exchange(0, std::memory_order_relaxed);
    uint64_t inferred  = ps.frames_inferred.exchange(0, std::memory_order_relaxed);
    uint64_t detections= ps.detections_total.exchange(0, std::memory_order_relaxed);
    uint64_t displayed = ps.frames_displayed.exchange(0, std::memory_order_relaxed);

    // 解码细分耗时统计
    uint64_t packets   = ps.packets_popped.exchange(0, std::memory_order_relaxed);
    uint64_t pop_us    = ps.decode_pop_wait_us.exchange(0, std::memory_order_relaxed);
    uint64_t send_us   = ps.decode_send_us.exchange(0, std::memory_order_relaxed);
    uint64_t recv_us   = ps.decode_receive_us.exchange(0, std::memory_order_relaxed);
    uint64_t upload_us = ps.decode_upload_us.exchange(0, std::memory_order_relaxed);
    uint64_t uploaded  = ps.frames_uploaded.exchange(0, std::memory_order_relaxed);
    uint64_t fq_wait_us= ps.framequeue_push_wait_us.exchange(0, std::memory_order_relaxed);
    uint64_t fq_pushed = ps.frames_pushed_fq.exchange(0, std::memory_order_relaxed);
    uint64_t fq_dropped= ps.frames_dropped_fq.exchange(0, std::memory_order_relaxed);

    // Worker 效率指标
    uint64_t w_pop_empty   = ps.worker_pop_empty.exchange(0, std::memory_order_relaxed);
    uint64_t w_batches     = ps.worker_batches_popped.exchange(0, std::memory_order_relaxed);
    uint64_t w_frames      = ps.worker_frames_popped.exchange(0, std::memory_order_relaxed);
    uint64_t w_slot_us     = ps.worker_slot_wait_us.exchange(0, std::memory_order_relaxed);
    uint64_t w_preproc_us  = ps.worker_preproc_us.exchange(0, std::memory_order_relaxed);
    uint64_t w_gpu_infer_us = ps.worker_gpu_infer_us.exchange(0, std::memory_order_relaxed);
    uint64_t w_gpu_batches  = ps.worker_gpu_batches.exchange(0, std::memory_order_relaxed);

    // GPU 显存查询
    size_t free_mem = 0, total_mem = 0;
    cudaMemGetInfo(&free_mem, &total_mem);
    double used_mb = static_cast<double>(total_mem - free_mem) / (1024.0 * 1024.0);
    double total_mb = static_cast<double>(total_mem) / (1024.0 * 1024.0);
    double usage_pct = (total_mem > 0) ? (100.0 * (total_mem - free_mem) / total_mem) : 0.0;

    // 输入就绪队列状态
    auto inputStats = InputFrameArenaStore::getInstance().getStats();
    size_t dq_size = inputStats.ready_frames;
    size_t dq_cap  = inputStats.max_ready_frames;

    // FrameQueue 各通道
    QString fqInfo;
    for (auto it = m_frameQueues.begin(); it != m_frameQueues.end(); ++it) {
        if (!fqInfo.isEmpty()) fqInfo += ", ";
        fqInfo += QString("ch%1=%2").arg(it.key()).arg(it.value()->size());
    }
    if (fqInfo.isEmpty()) fqInfo = "none";

    // Slot 池
    size_t slotsTotal = SlotPool::getInstance().totalSlots();
    size_t slotsAvail = SlotPool::getInstance().freeSlots();
    size_t slotsActive = SlotPool::getInstance().activeSlots();

    // 定时器周期，吞吐量换算为 /s
    double interval = m_pipelineStatsTimer ? m_pipelineStatsTimer->interval() / 1000.0 : 5.0;
    if (interval <= 0) interval = 5.0;
    double decode_fps  = decoded / interval;
    double push_fps    = pushed_dq / interval;
    double infer_fps   = inferred / interval;
    double display_fps = displayed / interval;
    double batch_ps    = batches / interval;

    // 解码细分平均耗时（ms）
    double avg_pop_wait_ms = (packets > 0) ? (pop_us / 1000.0) / packets : 0.0;
    double avg_send_ms     = (packets > 0) ? (send_us / 1000.0) / packets : 0.0;
    double avg_recv_ms     = (decoded > 0) ? (recv_us / 1000.0) / decoded : 0.0;
    double avg_upload_ms   = (uploaded > 0) ? (upload_us / 1000.0) / uploaded : 0.0;
    double avg_fq_push_ms  = (fq_pushed > 0) ? (fq_wait_us / 1000.0) / fq_pushed : 0.0;

    // Worker 效率计算
    double avg_batch = (w_batches > 0) ? static_cast<double>(w_frames) / w_batches : 0.0;
    double batch_util_pct = (m_workerMaxBatch > 0 && w_batches > 0) 
        ? 100.0 * avg_batch / m_workerMaxBatch : 0.0;
    uint64_t w_total_cycles = w_pop_empty + w_batches;
    double idle_pct = (w_total_cycles > 0) ? 100.0 * w_pop_empty / w_total_cycles : 0.0;
    double avg_slot_wait_ms = (w_batches > 0) ? (w_slot_us / 1000.0) / w_batches : 0.0;
    double avg_preproc_ms = (w_batches > 0) ? (w_preproc_us / 1000.0) / w_batches : 0.0;
    
    // [Baseline Calculation]
    double avg_gpu_infer_ms = (w_gpu_batches > 0) ? (w_gpu_infer_us / 1000.0) / w_gpu_batches : 0.0;
    double baseline_fps_avg = (avg_gpu_infer_ms > 0.001) ? (avg_batch * 1000.0 / avg_gpu_infer_ms) : 0.0;
    double baseline_fps_max = (avg_gpu_infer_ms > 0.001) ? (m_workerMaxBatch * 1000.0 / avg_gpu_infer_ms) : 0.0;

    // 瓶颈分析
    const char* bottleneck = "Unknown";
    const char* recommendation = "";
    double infer_ratio = (decode_fps > 1e-6) ? (infer_fps / decode_fps) : 1.0;
    double drop_ps = dropped_dq / interval;
    if (decode_fps < 1.0 && infer_fps < 1.0) {
        bottleneck = "Idle (no active channels)";
        recommendation = "";
    } else if (infer_ratio < 0.92 && (drop_ps > 1.0 || (dq_cap > 0 && dq_size > dq_cap * 0.2) || batch_util_pct < 45.0)) {
        bottleneck = "INFERENCE-PARALLELISM-LIMITED (推理并行不足)";
        recommendation = "Infer trails input. Increase active workers or improve micro-batching utilization.";
    } else if (idle_pct > 65.0 && infer_ratio >= 0.92) {
        bottleneck = "INPUT-LIMITED (输入供给不足)";
        recommendation = "Workers mostly idle while infer keeps up. Increase active channels or source FPS.";
    } else if (avg_slot_wait_ms > 5.0) {
        bottleneck = "SLOT-LIMITED (显存槽瓶颈)";
        recommendation = "Workers blocking on slot acquire. Increase SlotPool size or reduce parallelism/batch.";
    } else {
        bottleneck = "BALANCED (均衡)";
        recommendation = "Pipeline is well balanced.";
    }

    fprintf(stderr,
        "\n"
        "╔══════════════════════════════════════════════════════════════╗\n"
        "║              Pipeline Diagnostic Report                     ║\n"
        "╠══════════════════════════════════════════════════════════════╣\n"
        "║ GPU Memory:    %.0f / %.0f MiB (%.1f%%)                     \n"
        "║ Slot Pool:     Active %zu / %zu  (Free: %zu)               \n"
        "║ Input ReadyQ:  %zu / %zu frames                             \n"
        "║ FrameQueues:   %s                                           \n"
        "║ Workers:       %d active, streams=%d, maxBatch=%d           \n"
        "╠══════════════════════════════════════════════════════════════╣\n"
        "║ Throughput (per second, %.0fs interval):                    \n"
        "║   Input:       %.1f fps                                     \n"
        "║   Input Push:  %.1f fps (dropped: %.1f/s)                   \n"
        "║   Inference:   %.1f fps (%.1f batches/s)                    \n"
        "║   Detections:  %.1f objects/s                               \n"
        "║   Display:     %.1f fps                                     \n"
        "╠══════════════════════════════════════════════════════════════╣\n"
        "║ Decoder Timing (avg):                                       \n"
        "║   Packet wait: %.3f ms (packets=%llu)                       \n"
        "║   Send packet: %.3f ms                                      \n"
        "║   Receive frame: %.3f ms                                    \n"
        "║   CPU->GPU upload: %.3f ms (frames=%llu)                    \n"
        "║   FrameQueue push: %.3f ms (dropped=%llu)                   \n"
        "╠══════════════════════════════════════════════════════════════╣\n"
        "║ Worker Efficiency:                                          \n"
        "║   Idle cycles: %llu / %llu (%.1f%% idle — waiting for data) \n"
        "║   Avg batch:   %.1f / %d (%.1f%% utilization)               \n"
        "║   Slot wait:   %.2f ms avg per batch                        \n"
        "║   Preprocess:  %.2f ms avg per batch                        \n"
        "║   Infer Time:  %.2f ms avg (per batch)                      \n"
        "║   Baseline:    ~%.0f FPS (cur batch) / ~%.0f FPS (max batch)\n"
        "╠══════════════════════════════════════════════════════════════╣\n"
        "║ ⚠ BOTTLENECK:  %s\n"
        "║   %s\n"
        "╚══════════════════════════════════════════════════════════════╝\n",
        used_mb, total_mb, usage_pct,
        slotsActive, slotsTotal, slotsAvail,
        dq_size, dq_cap,
        fqInfo.toUtf8().constData(),
        m_workerCount, m_inferenceStreams, m_workerMaxBatch,
        interval,
        decode_fps,
        push_fps, dropped_dq / interval,
        infer_fps, batch_ps,
        detections / interval,
        display_fps,
        avg_pop_wait_ms, (unsigned long long)packets,
        avg_send_ms,
        avg_recv_ms,
        avg_upload_ms, (unsigned long long)uploaded,
        avg_fq_push_ms, (unsigned long long)fq_dropped,
        (unsigned long long)w_pop_empty, (unsigned long long)w_total_cycles, idle_pct,
        avg_batch, m_workerMaxBatch, batch_util_pct,
        avg_slot_wait_ms,
        avg_preproc_ms,
        avg_gpu_infer_ms, baseline_fps_avg, baseline_fps_max,
        bottleneck,
        recommendation
    );
}

// ============ 高级设置 ============

void MainWindow::openAdvancedSettings()
{
    if (!m_advancedDialog) {
    // 不挂在 MainWindow 样式树下，避免继承 mainwindow.ui 的全局深色 QWidget 样式
    m_advancedDialog = new AdvancedSettingsDialog(nullptr);
    m_advancedDialog->setAttribute(Qt::WA_StyledBackground, true);
        connect(m_advancedDialog, &AdvancedSettingsDialog::settingsApplied,
                this, &MainWindow::applySettings);

        // 压力测试：启动时暂停所有通道 + 自动初始化检测管线
        connect(m_advancedDialog, &AdvancedSettingsDialog::loadTestStartRequested, this,
            [this](bool useRealDecode, int numChannels, int targetFps, int layoutMode) {
                m_loadTestRealDecode = useRealDecode;
            if (m_nvtxLoadTestRangeId != 0) {
                nvtxRangeEnd(m_nvtxLoadTestRangeId);
                m_nvtxLoadTestRangeId = 0;
            }
            // NVTX: start a named range for the load test so captures can be scoped
                m_nvtxLoadTestRangeId = nvtxRangeStartA("LoadTest");
            m_loadTestChannels.clear();
            m_loadTestTargetFps = targetFps;
            m_loadTestPrevSettings.clear();
            m_loadTestPrevRunning.clear();

            if (!useRealDecode) {
                // 注入模式：暂停所有运行中的解码器
                for (auto it = m_decoders.begin(); it != m_decoders.end(); ++it) {
                    it.value()->setPaused(true);
                    // 压测：关闭低帧率限制，尽量放开解码吞吐
                    it.value()->setLowFPSMode(false);
                    it.value()->setAllowOverNativeFPS(true);
                    it.value()->setTargetFPS(120);
                    // 文件源支持加速读取，提升解码供给
                    it.value()->setSpeed(4.0f);
                }
            } else {
                // 真实解码模式：保持通道运行，仅放开限速并按目标 FPS 设置倍速
                if (layoutMode >= 1 && layoutMode <= 3) {
                    ui->comboBox_layout->setCurrentIndex(layoutMode - 1);
                }
                const QString testPath = "test/output.mp4";
                for (int ch = 0; ch < numChannels; ++ch) {
                    // 备份原通道设置/状态
                    if (m_channelSettings.contains(ch)) {
                        m_loadTestPrevSettings.insert(ch, m_channelSettings.value(ch));
                    }
                    if (m_decoders.contains(ch)) {
                        m_loadTestPrevRunning.insert(ch);
                    }

                    // 设置为文件源并启动
                    m_channelSettings[ch].sourceTypeIndex = 0;
                    m_channelSettings[ch].sourcePath = testPath;
                    if (!m_decoders.contains(ch)) {
                        startChannel(ch);
                    }

                    if (m_decoders.contains(ch)) {
                        VideoDecoder* d = m_decoders[ch];
                        m_loadTestChannels.push_back(ch);
                        d->setPaused(false);
                        d->setLowFPSMode(false);
                        d->setAllowOverNativeFPS(true);
                        d->setTargetFPS(targetFps);
                        double native = d->getNativeFPS();
                        float speed = 1.0f;
                        if (native > 1e-3) {
                            double ratio = static_cast<double>(targetFps) / native;
                            if (ratio > 1.0) speed = static_cast<float>(ratio);
                        }
                        d->setSpeed(speed);
                        // 启动压测时刷新 Epoch，且应用到 Decoder
                        // 这样 Worker 在回调时会携带此 Epoch，DetectionResults 只接受匹配的结果
                        uint64_t newEpoch = DetectionResults::getInstance().bumpEpoch(ch);
                        d->setChannelEpoch(newEpoch);
                    }
                }
            }

            // 如果检测管线没启动，自动启动
            if (!m_detectionEnabled) {
                startDetection();
            }
            logSystemMessage(useRealDecode
                ? "Load test: real decode mode enabled."
                : "Load test: channels paused, detection pipeline ready.", 0);
        });

        // 压力测试：结束后恢复所有通道并释放多余显存
        connect(m_advancedDialog, &AdvancedSettingsDialog::loadTestStopRequested, this, [this]() {
            // NVTX: end the load test range if it was started
            if (m_nvtxLoadTestRangeId) {
                nvtxRangeEnd(m_nvtxLoadTestRangeId);
                m_nvtxLoadTestRangeId = 0;
            }
            if (m_loadTestRealDecode) {
                // 遍历 m_loadTestChannels 而非 PrevSettings，确保所有参与过压测的通道都被处理
                // 之前使用的 m_loadTestPrevSettings 可能不包含那些压测前“未配置”的通道，导致它们无法被停止
                for (int ch : m_loadTestChannels) {
                    // 1. 恢复配置 (m_channelSettings)
                    if (m_loadTestPrevSettings.contains(ch)) {
                        m_channelSettings[ch] = m_loadTestPrevSettings.value(ch);
                    } else if (m_channelSettings.contains(ch)) {
                        // 压测前无配置的通道，重置为空，避免残留 "test/output.mp4" 路径
                        m_channelSettings[ch] = ChannelSettings();
                    }

                    // 2. 强制停止当前测试流 (清理 decoder/widget/results)
                    // 这一步至关重要：必须销毁 VideoDecoder 才能解除对 test/output.mp4 的占用
                    // 且 stopChannel 会触发 VideoWidget::clear，消除残留画面和检测框
                    stopChannel(ch);

                    // 停止后立即刷新 Epoch，使管道中残留的旧 epoch 结果失效 (Worker updateIfCurrent 将拒绝写入)
                    DetectionResults::getInstance().bumpEpoch(ch);

                    // [再次强制清理] 确保检测结果被彻底清除, 即使 stopChannel 已经清理过，保险起见
                    // 因为在检测线程中可能还在残留上一帧的结果
                    if (VideoWidget* v = findVideoWidget(ch)) {
                        v->clear(); 
                    }
                    DetectionResults::getInstance().clear(ch);
                    ChannelResultQueue::getInstance().clearChannel(ch);

                    // 3. 恢复之前的运行状态
                    // 如果压测前该通道是运行的，则使用恢复后的 m_channelSettings 重新启动
                    if (m_loadTestPrevRunning.contains(ch)) {
                        startChannel(ch);
                    }
                }
                
                m_loadTestChannels.clear();
                m_loadTestPrevSettings.clear();
                m_loadTestPrevRunning.clear();
                m_loadTestRealDecode = false;
            } else {
                for (auto it = m_decoders.begin(); it != m_decoders.end(); ++it) {
                    it.value()->setPaused(false);
                    // 压测结束：恢复低帧率模式
                    it.value()->setLowFPSMode(true);
                    it.value()->setAllowOverNativeFPS(false);
                    it.value()->setTargetFPS(30);
                    it.value()->setSpeed(1.0f);
                }
            }
            logSystemMessage("Load test finished.", 0);
        });
    }
    // 每次打开时同步当前值到 dialog
    AdvancedSettingsDialog::Settings cur;
    cur.baseSlots      = m_baseSlots;
    cur.inputArenaFrames = m_inputArenaFrames;
    cur.workerCount    = std::max(1, m_inferenceStreams);
    cur.inferenceStreams = m_inferenceStreams;
    cur.workerMaxBatch = m_workerMaxBatch;
    cur.modelPath      = m_modelPath;
    cur.classesPath    = m_classesPath;
    cur.statsInterval  = m_pipelineStatsTimer ? m_pipelineStatsTimer->interval() / 1000 : 5;
    m_advancedDialog->setSettings(cur);
    m_advancedDialog->show();
    m_advancedDialog->raise();
    m_advancedDialog->activateWindow();
}

void MainWindow::applySettings(const AdvancedSettingsDialog::Settings& s)
{
    m_baseSlots        = s.baseSlots;
    m_inputArenaFrames = std::max(16, s.inputArenaFrames);
    m_inferenceStreams = std::max(1, s.inferenceStreams);
    m_workerMaxBatch   = s.workerMaxBatch;
    m_modelPath        = s.modelPath;

    // 如果 classesPath 变更，清空已加载的类别名称以触发重新加载
    if (m_classesPath != s.classesPath) {
        m_classesPath = s.classesPath;
        m_classNames.clear();
        // 同步到 VideoWidget 的静态类别路径，使绘制检测框时使用新路径
        VideoWidget::setClassesFilePath(m_classesPath);
    }

    if (m_pipelineStatsTimer) {
        m_pipelineStatsTimer->setInterval(s.statsInterval * 1000);
    }

    // 检测运行中时热更新 Worker 数量
    if (m_detectionEnabled) {
        int desiredWorkers = std::max(1, m_inferenceStreams);
        const bool worker_changed = (m_workerCount != desiredWorkers);
        if (worker_changed || !m_workers.empty()) {
            logSystemMessage(QString("Hot-applying parallel workers: %1 -> %2")
                .arg(m_workerCount).arg(desiredWorkers), 0);
            for (auto &w : m_workers) w->stop();
            m_workers.clear();
            m_workerCount = desiredWorkers;
            for (int i = 0; i < m_workerCount; ++i) {
                auto w = std::make_unique<Worker>(
                    i,
                    static_cast<size_t>(m_workerMaxBatch),
                    std::chrono::milliseconds(5));
                w->start();
                m_workers.push_back(std::move(w));
            }
        }
    }

    int effectiveBatch = std::min(m_workerMaxBatch, TRTDetector::getInstance().getMaxBatch());
    logSystemMessage(QString("Settings applied — slots=%1, parallelWorkers=%2, batch=%3, model=%4")
        .arg(recommendedSlotCount(effectiveBatch)).arg(std::max(1, s.inferenceStreams))
        .arg(s.workerMaxBatch).arg(s.modelPath), 0);
}

void MainWindow::startChannel(int channel_id)
{
    if (m_decoders.contains(channel_id)) {
        logSystemMessage(QString("Channel %1 is already running.").arg(channel_id + 1), 1);
        return;
    }

    QString url = m_channelSettings[channel_id].sourcePath;
    if (url.isEmpty()) {
        logSystemMessage(QString("Channel %1 source not configured.").arg(channel_id + 1), 2);
        return;
    }

    // 针对 USB 摄像头 (Index 2)，如果用户输入的是数字索引 (0, 1...)，自动转换为 Linux 设备路径 (/dev/video0)
    if (m_channelSettings[channel_id].sourceTypeIndex == 2) {
        bool ok;
        int devIndex = url.toInt(&ok);
        if (ok) {
            url = QString("/dev/video%1").arg(devIndex);
        }
    }

    // 1. 创建队列、解码器和线程
    FrameQueue* queue = new FrameQueue(5); // 缓冲 5 帧
    m_frameQueues[channel_id] = queue;
    m_displayManager->addChannel(channel_id, queue);

    // 绑定 VideoWidget 到 DisplayWorker 的数据源
    DisplayWorker* worker = m_displayManager->getWorker(channel_id);
    if (worker) {
        if (VideoWidget* v = findVideoWidget(channel_id)) {
            v->setDataSource(&worker->current_ptr, &worker->current_w,
                             &worker->current_h, &worker->current_pitch,
                             &worker->current_format);
        }
    }

    VideoDecoder* decoder = new VideoDecoder(url.toStdString(), channel_id, queue);
    QThread* thread = new QThread;

    // 递增通道 epoch，确保旧的异步推理回调不会覆盖新结果
    uint64_t newEpoch = DetectionResults::getInstance().bumpEpoch(channel_id);
    decoder->setChannelEpoch(newEpoch);
    // 清除旧的当前帧检测结果（无论源类型，避免显示旧源的检测框）
    DetectionResults::getInstance().clear(channel_id);
    ChannelResultQueue::getInstance().clearChannel(channel_id);

    decoder->moveToThread(thread);

    // 连接信号槽
    connect(thread, &QThread::started, decoder, &VideoDecoder::startDecoding);
    connect(decoder, &VideoDecoder::playbackFinished, this, &MainWindow::onPlaybackFinished, Qt::QueuedConnection);
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);

    m_decoders[channel_id] = decoder;
    m_threads[channel_id] = thread;

    // 允许该通道进入检测队列
    InputFrameArenaStore::getInstance().enableChannel(channel_id);

    // 如果检测正在运行且是图片源，确保队列已就绪并记录诊断信息
    if (m_detectionEnabled && m_channelSettings[channel_id].sourceTypeIndex == 3) {
        fprintf(stderr, "[MainWindow] Image channel %d started with detection enabled. "
                "SlotQ enabled=%d, channel_enabled=%d, workers=%zu\n",
                channel_id,
                InputFrameArenaStore::getInstance().isEnabled() ? 1 : 0,
                InputFrameArenaStore::getInstance().isChannelEnabled(channel_id) ? 1 : 0,
                m_workers.size());
    }

    // 初始化渲染状态
    if (worker) {
        bool shouldRender = !m_isAnalysisMode || (channel_id == m_currentChannelIndex);
        worker->setRenderingEnabled(shouldRender);
    }

    thread->start();

    // 记录源切换历史（用于报告）
    if (m_detectionEnabled) {
        static const char* typeNames[] = {"Video", "RTSP", "USB", "Image"};
        int ti = m_channelSettings[channel_id].sourceTypeIndex;
        QString entry = QString("[%1] %2: %3")
            .arg(QDateTime::currentDateTime().toString("HH:mm:ss"))
            .arg(ti >= 0 && ti <= 3 ? typeNames[ti] : "?")
            .arg(url);
        m_sourceHistory[channel_id].append(entry);
    }

    logSystemMessage(QString("Started channel %1: %2").arg(channel_id + 1).arg(url));
}

void MainWindow::stopChannel(int channel_id)
{
    if (!m_decoders.contains(channel_id)) return;

    // 禁止该通道进入检测队列并清理队列中的残留
    InputFrameArenaStore::getInstance().disableChannel(channel_id);
    InputFrameArenaStore::getInstance().clearChannel(channel_id);
    
    // 先清除 VideoWidget 的显示，防止野指针
    if (VideoWidget* v = findVideoWidget(channel_id)) {
        v->clear();
    }

    VideoDecoder* decoder = m_decoders.take(channel_id);
    decoder->stopDecoding();
    
    QThread* thread = m_threads.take(channel_id);
    thread->quit();
    thread->wait();
    
    delete decoder;
    
    m_displayManager->removeChannel(channel_id);
    delete m_frameQueues.take(channel_id);

    // 始终清除当前帧结果（不影响累计统计，报告不受影响）
    DetectionResults::getInstance().clear(channel_id);
    ChannelResultQueue::getInstance().clearChannel(channel_id);
    
    logSystemMessage(QString("Stopped channel %1").arg(channel_id + 1));
}

