#include "AdvancedSettingsDialog.hpp"
#include "ui_AdvancedSettingsDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QSizeGrip>
#include <QFileDialog>
#include <QTabWidget>
#include <QScrollArea>
#include <QFont>
#include <QFontMetrics>
#include <QStyle>
#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QComboBox>
#include <QDebug>
#include <QPainter>
#include <QTextDocumentFragment>
#include <QMouseEvent>
#include <QEvent>
#include <QGraphicsDropShadowEffect>
#include <cstdio>
#include <thread>
#include <memory>
#include <algorithm>
#include <cmath>
#include <type_traits>

#include "src/core/SlotPool.hpp"
#include "src/core/InputFrameArenaStore.hpp"
#include "src/core/TensorArenaManager.hpp"
#include "src/core/PipelineStats.hpp"
#include "src/core/FrameQueue.hpp"
#include "src/engine/TRTDetector.hpp"
#include "src/video/VideoDecoder.hpp"
#include "src/video/DisplayManager.hpp"

#include <cuda_runtime.h>

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

static constexpr const char* ORG  = "CudaForge";
static constexpr const char* APP  = "CudaForge-YOLO";

static QString toPlainReportText(const QString& text) {
    if (text.contains('<') && text.contains('>')) {
        return QTextDocumentFragment::fromHtml(text).toPlainText().trimmed();
    }
    return text;
}

class ArenaStateBar final : public QWidget {
public:
    struct Segment {
        double begin = 0.0;
        double end = 0.0;
        QColor color;
    };

    explicit ArenaStateBar(QWidget* parent = nullptr) : QWidget(parent) {
        setMinimumHeight(16);
        setMaximumHeight(18);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    void setSegments(const std::vector<Segment>& segs) {
        segments_ = segs;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, false);
        QRect r = rect().adjusted(0, 0, -1, -1);
        painter.fillRect(r, QColor("#F8FAFC"));

        const int width = r.width();
        for (const auto& s : segments_) {
            if (s.end <= s.begin) {
                continue;
            }
            int x0 = r.left() + static_cast<int>(std::floor(s.begin * width));
            int x1 = r.left() + static_cast<int>(std::ceil(s.end * width));
            if (x1 <= x0) {
                x1 = x0 + 1;
            }
            painter.fillRect(QRect(x0, r.top(), x1 - x0, r.height()), s.color);
        }

        painter.setPen(QColor("#64748B"));
        painter.drawRect(r);
    }

private:
    std::vector<Segment> segments_;
};

AdvancedSettingsDialog::Settings AdvancedSettingsDialog::defaultSettings()
{
    Settings s;
    s.baseSlots      = 4;
    s.inputArenaFrames = 200;
    s.outputArenaFrames = 200;
    s.workerCount    = 2;
    s.inferenceStreams = 2;
    s.workerMaxBatch = 16;
    s.contextPoolSize = 1;
    s.modelPath      = "/home/zzx/code/Qt/CudaForge-YOLO/src/engines/yolo26n.engine";
    s.classesPath    = "src/engines/class.txt";
    s.statsInterval  = 5;
    s.displayConfThreshold = 0.55;
    return s;
}

AdvancedSettingsDialog::Settings AdvancedSettingsDialog::loadFromDisk()
{
    QSettings qs(ORG, APP);
    Settings def = defaultSettings();
    Settings s;
    s.baseSlots      = qs.value("baseSlots", def.baseSlots).toInt();
    s.inputArenaFrames = qs.value("inputArenaFrames", def.inputArenaFrames).toInt();
    s.outputArenaFrames = qs.value("outputArenaFrames", def.outputArenaFrames).toInt();
    s.inferenceStreams = qs.value("inferenceStreams", def.inferenceStreams).toInt();
    s.workerCount    = std::max(1, s.inferenceStreams);
    s.workerMaxBatch = qs.value("workerMaxBatch", def.workerMaxBatch).toInt();
    (void)qs.value("contextPoolSize", def.contextPoolSize).toInt();
    s.contextPoolSize = 1;
    s.modelPath      = qs.value("modelPath", def.modelPath).toString();
    s.classesPath    = qs.value("classesPath", def.classesPath).toString();
    s.statsInterval  = qs.value("statsInterval", def.statsInterval).toInt();
    s.displayConfThreshold = qs.value("displayConfThreshold", def.displayConfThreshold).toDouble();
    return s;
}

void AdvancedSettingsDialog::saveToDisk(const Settings& s)
{
    QSettings qs(ORG, APP);
    qs.setValue("baseSlots", s.baseSlots);
    qs.setValue("inputArenaFrames", s.inputArenaFrames);
    qs.setValue("outputArenaFrames", s.outputArenaFrames);
    qs.setValue("workerCount", std::max(1, s.inferenceStreams));
    qs.setValue("inferenceStreams", std::max(1, s.inferenceStreams));
    qs.setValue("workerMaxBatch", s.workerMaxBatch);
    qs.setValue("contextPoolSize", 1);
    qs.setValue("modelPath", s.modelPath);
    qs.setValue("classesPath", s.classesPath);
    qs.setValue("statsInterval", s.statsInterval);
    qs.setValue("displayConfThreshold", s.displayConfThreshold);
    qs.sync();
}

AdvancedSettingsDialog::AdvancedSettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setObjectName("advancedSettingsDialog");
    Qt::WindowFlags flags = windowFlags();
    flags |= Qt::FramelessWindowHint;
    flags &= ~Qt::WindowStaysOnTopHint;
    flags &= ~Qt::WindowContextHelpButtonHint;
    setWindowFlags(flags);
    setWindowTitle("Advanced Settings");
    setMinimumSize(900, 620);
    resize(1280, 820);
    buildUI();

    m_dashTimer = new QTimer(this);
    connect(m_dashTimer, &QTimer::timeout, this, &AdvancedSettingsDialog::refreshDashboard);
    m_dashTimer->start(2000);
    refreshDashboard();
}

AdvancedSettingsDialog::~AdvancedSettingsDialog()
{
    delete m_ui;
    m_ui = nullptr;
}

bool AdvancedSettingsDialog::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_titleBar || watched == m_titleLabel) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto *me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                m_dragging = true;
                m_dragOffset = me->globalPosition().toPoint() - frameGeometry().topLeft();
                return true;
            }
        } else if (event->type() == QEvent::MouseMove && m_dragging) {
            auto *me = static_cast<QMouseEvent*>(event);
            move(me->globalPosition().toPoint() - m_dragOffset);
            return true;
        } else if (event->type() == QEvent::MouseButtonRelease) {
            m_dragging = false;
            return true;
        }
    }
    return QDialog::eventFilter(watched, event);
}

void AdvancedSettingsDialog::applyModernStyle()
{
    QString lightStyle = R"(
        QDialog#advancedSettingsDialog {
            background-color: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                stop:0 #edf3f1,
                stop:0.5 #e6efec,
                stop:1 #dde8e4);
            color: #334155;
            font-size: 12px;
        }
        QDialog#advancedSettingsDialog QWidget {
            color: #334155;
            font-size: 12px;
        }
        QWidget#customTitleBar {
            background: #dfe9e5;
            border: none;
            border-bottom: 2px solid #c5d7d0;
            border-radius: 0px;
        }
        QWidget#dialogBody {
            background: rgba(229, 237, 234, 0.78);
        }
        QWidget#dashPage {
            background: rgba(217, 228, 224, 0.78);
            border: 1px solid #c5d3cf;
            border-radius: 10px;
        }
        QWidget#tabSettings {
            background: rgba(221, 231, 227, 0.72);
            border: 1px solid #c9d8d2;
            border-radius: 10px;
        }
        QLabel#customTitleLabel {
            color: #2f4350;
            font-weight: 800;
            font-size: 14px;
            letter-spacing: 0.6px;
            background: rgba(226, 239, 233, 0.9);
            border: 1px solid #b9cdc6;
            border-radius: 8px;
            padding: 4px 10px;
        }
        QPushButton#titleCloseButton {
            min-width: 22px;
            max-width: 22px;
            min-height: 22px;
            max-height: 22px;
            border-radius: 5px;
            background: #fbe3e1;
            border: 1px solid #edc9c5;
            color: #B91C1C;
            font-weight: 700;
            font-size: 12px;
            padding: 0px;
        }
        QPushButton#titleCloseButton:hover {
            background: #f7d4d0;
            border: 1px solid #e5b9b2;
        }
        QWidget[card="true"] {
            background-color: rgba(241, 249, 245, 0.95);
            border: 1px solid #b8cbc4;
            border-top: 2px solid #90b8ac;
            border-radius: 12px;
        }
        QLabel[role="cardTitle"] {
            color: #2a434f;
            font-size: 14px;
            font-weight: 800;
            padding: 3px 8px;
            background: rgba(225, 238, 232, 0.95);
            border: 1px solid #b4c9c1;
            border-radius: 7px;
        }
        QLabel[role="rowTitle"] {
            color: #64748b;
            font-size: 12px;
        }
        QLabel {
            color: #475569;
            font-size: 12px;
        }
        QComboBox, QSpinBox, QLineEdit {
            background-color: rgba(255,255,255,0.82);
            border: 1px solid #d7e5df;
            border-radius: 6px;
            padding: 4px;
            color: #334155;
            font-size: 12px;
        }
        QPushButton {
            background-color: rgba(255,255,255,0.82);
            border: 1px solid #d7e5df;
            border-radius: 6px;
            padding: 6px 14px;
            color: #334155;
            font-size: 12px;
            font-weight: 600;
        }
        QPushButton:hover {
            background-color: rgba(240,248,245,0.95);
            border: 1px solid #b7d1c9;
            color: #2f4b45;
        }
        QProgressBar {
            background-color: rgba(238,245,242,0.9);
            border: 1px solid #d7e5df;
            border-radius: 6px;
            text-align: center;
            color: #334155;
            font-size: 12px;
            min-height: 18px;
        }
        QProgressBar::chunk {
            background-color: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #9bc8bb, stop:1 #7bb4a5);
            border-radius: 5px;
        }
        QTabWidget::pane {
            border: 1px solid #c5d6d0;
            border-radius: 8px;
            background-color: transparent;
            margin-top: 2px;
        }
        QTabBar::tab {
            background-color: transparent;
            color: #64748b;
            padding: 8px 18px;
            border: none;
            border-bottom: 2px solid transparent;
            font-weight: 600;
            font-size: 12px;
        }
        QTabBar::tab:selected {
            color: #2f5f54;
            border-bottom: 2px solid #8ebfb1;
        }
        QScrollArea,
        QScrollArea > QWidget,
        QScrollArea > QWidget > QWidget,
        QWidget#paramPage {
            background-color: transparent;
        }
    )";
    this->setStyleSheet(lightStyle);
}

void AdvancedSettingsDialog::buildUI()
{
    applyModernStyle();

    if (!m_ui) {
        m_ui = new Ui::AdvancedSettingsDialog;
        m_ui->setupUi(this);
    }

    m_titleBar = m_ui->customTitleBar;
    m_titleLabel = m_ui->lblTitle;
    m_btnCloseDialog = m_ui->btnClose;

    auto req = [this](auto dummy, const char* name) {
        using T = decltype(dummy);
        T ptr = this->findChild<std::remove_pointer_t<T>*>(name);
        if (!ptr) {
            qFatal("Missing UI object: %s", name);
        }
        return ptr;
    };

    m_spinSlots       = req(static_cast<QSpinBox*>(nullptr), "m_spinSlots");
    m_spinWorkerCount = req(static_cast<QSpinBox*>(nullptr), "m_spinWorkerCount");
    m_spinInferenceStreams = req(static_cast<QSpinBox*>(nullptr), "m_spinInferenceStreams");
    m_spinBatch       = req(static_cast<QSpinBox*>(nullptr), "m_spinBatch");
    m_spinContextPool = req(static_cast<QSpinBox*>(nullptr), "m_spinContextPool");
    m_spinStatsInterval = req(static_cast<QSpinBox*>(nullptr), "m_spinStatsInterval");
    m_editModelPath   = req(static_cast<QLineEdit*>(nullptr), "m_editModelPath");
    m_editClassesPath = req(static_cast<QLineEdit*>(nullptr), "m_editClassesPath");
    m_btnBrowseModel  = req(static_cast<QPushButton*>(nullptr), "m_btnBrowseModel");
    m_btnBrowseClasses= req(static_cast<QPushButton*>(nullptr), "m_btnBrowseClasses");
    m_btnApply        = req(static_cast<QPushButton*>(nullptr), "m_btnApply");
    m_btnDefaults     = req(static_cast<QPushButton*>(nullptr), "m_btnDefaults");

    m_lblGpuMem       = req(static_cast<QLabel*>(nullptr), "m_lblGpuMem");
    m_barGpuMem       = req(static_cast<QProgressBar*>(nullptr), "m_barGpuMem");
    m_lblVramSlot     = req(static_cast<QLabel*>(nullptr), "m_lblVramSlot");
    m_lblVramOther    = req(static_cast<QLabel*>(nullptr), "m_lblVramOther");
    m_lblVramCtx      = req(static_cast<QLabel*>(nullptr), "m_lblVramCtx");
    m_lblVramDecoder  = req(static_cast<QLabel*>(nullptr), "m_lblVramDecoder");
    m_lblDecoderCount = req(static_cast<QLabel*>(nullptr), "m_lblDecoderCount");
    m_lblInputArena   = req(static_cast<QLabel*>(nullptr), "m_lblInputArena");
    m_lblOutputArena  = req(static_cast<QLabel*>(nullptr), "m_lblOutputArena");
    m_lblSlotPool     = req(static_cast<QLabel*>(nullptr), "m_lblSlotPool");
    m_barSlotPool     = req(static_cast<QProgressBar*>(nullptr), "m_barSlotPool");
    m_lblDetQueue     = req(static_cast<QLabel*>(nullptr), "m_lblDetQueue");
    m_barDetQueue     = req(static_cast<QProgressBar*>(nullptr), "m_barDetQueue");
    m_lblDecodeFps    = req(static_cast<QLabel*>(nullptr), "m_lblDecodeFps");
    m_lblInferFps     = req(static_cast<QLabel*>(nullptr), "m_lblInferFps");
    m_lblDisplayFps   = req(static_cast<QLabel*>(nullptr), "m_lblDisplayFps");
    m_lblDetections   = req(static_cast<QLabel*>(nullptr), "m_lblDetections");
    m_lblDqDrop       = req(static_cast<QLabel*>(nullptr), "m_lblDqDrop");
    m_lblDqPush       = req(static_cast<QLabel*>(nullptr), "m_lblDqPush");
    m_lblBatchUtil    = req(static_cast<QLabel*>(nullptr), "m_lblBatchUtil");
    m_lblWorkerIdle   = req(static_cast<QLabel*>(nullptr), "m_lblWorkerIdle");
    m_lblSlotWait     = req(static_cast<QLabel*>(nullptr), "m_lblSlotWait");
    m_lblPreprocTime  = req(static_cast<QLabel*>(nullptr), "m_lblPreprocTime");
    m_lblCtxPool      = req(static_cast<QLabel*>(nullptr), "m_lblCtxPool");
    m_lblPeakSlots    = req(static_cast<QLabel*>(nullptr), "m_lblPeakSlots");
    m_lblBottleneck   = req(static_cast<QLabel*>(nullptr), "m_lblBottleneck");
    m_lblLoadTest     = req(static_cast<QLabel*>(nullptr), "m_lblLoadTest");
    m_btnLoadTest     = req(static_cast<QPushButton*>(nullptr), "m_btnLoadTest");
    m_btnStopTest     = req(static_cast<QPushButton*>(nullptr), "m_btnStopTest");
    m_btnCopyDash     = req(static_cast<QPushButton*>(nullptr), "m_btnCopyDash");
    m_comboLoadLayout = req(static_cast<QComboBox*>(nullptr), "m_comboLoadLayout");
    m_lblLoadLayout   = req(static_cast<QLabel*>(nullptr), "m_lblLoadLayout");
    m_spinLoadFps     = req(static_cast<QSpinBox*>(nullptr), "m_spinLoadFps");
    m_spinLoadDuration= req(static_cast<QSpinBox*>(nullptr), "m_spinLoadDuration");

    if (auto *paramsForm = this->findChild<QFormLayout*>("formPipelineParams")) {
        m_spinDisplayConf = new QDoubleSpinBox(this);
        m_spinDisplayConf->setRange(0.01, 1.00);
        m_spinDisplayConf->setSingleStep(0.01);
        m_spinDisplayConf->setDecimals(2);
        m_spinDisplayConf->setValue(0.55);
        m_spinDisplayConf->setToolTip("Display/filter confidence threshold for detections.");
        auto *lbl = new QLabel("Display Conf / 显示置信度:", this);
        lbl->setProperty("role", "rowTitle");
        paramsForm->insertRow(6, lbl, m_spinDisplayConf);
    }

    if (auto *arenaForm = this->findChild<QFormLayout*>("formArena")) {
        m_barInputArenaState = new ArenaStateBar(this);
        m_barOutputArenaState = new ArenaStateBar(this);
        m_lblInputArenaStates = new QLabel("--", this);
        m_lblOutputArenaStates = new QLabel("--", this);
        m_lblInputArenaStates->setWordWrap(true);
        m_lblOutputArenaStates->setWordWrap(true);
        m_lblInputArenaStates->setStyleSheet("QLabel { color:#475569; font-size:11px; }");
        m_lblOutputArenaStates->setStyleSheet("QLabel { color:#475569; font-size:11px; }");

        arenaForm->insertRow(1, new QLabel("Input Regions:", this), m_barInputArenaState);
        arenaForm->insertRow(2, new QLabel("Input States:", this), m_lblInputArenaStates);
        arenaForm->insertRow(4, new QLabel("Output Regions:", this), m_barOutputArenaState);
        arenaForm->insertRow(5, new QLabel("Output States:", this), m_lblOutputArenaStates);
    }

    // 并行度参数统一：Worker 数与推理并行数绑定，不再单独开放 Worker Count
    m_spinWorkerCount->setMinimum(1);
    m_spinWorkerCount->setMaximum(1);
    m_spinWorkerCount->setValue(1);
    m_spinWorkerCount->setEnabled(false);
    m_spinWorkerCount->setToolTip("Worker count follows Parallel Infer");
    if (auto *workerLabel = this->findChild<QLabel*>("txtWorkers")) {
        workerLabel->hide();
    }
    m_spinWorkerCount->hide();

    // 将该参数用于 Input Arena 帧数
    m_spinSlots->setMinimum(16);
    m_spinSlots->setMaximum(2000);
    m_spinSlots->setSingleStep(16);
    m_spinSlots->setToolTip("Input arena frames");

    // 复用该参数位为 Output Arena 帧数
    m_spinContextPool->setMinimum(16);
    m_spinContextPool->setMaximum(2000);
    m_spinContextPool->setSingleStep(16);
    m_spinContextPool->setToolTip("Output arena frames");
    m_spinInferenceStreams->setToolTip("Parallel infer workers (1 worker = 1 stream + 1 TRT context)");
    if (auto *ctxLabel = this->findChild<QLabel*>("txtCtxPoolParam")) {
        ctxLabel->setText("Output Arena Frames / 输出池帧数:");
    }
    if (auto *ctxRowTitle = this->findChild<QLabel*>("txtCtx")) {
        ctxRowTitle->setText("Worker Failures / 失败计数:");
    }
    if (auto *titleDetQueue = this->findChild<QLabel*>("titleDetQueue")) {
        titleDetQueue->setText("Input Ready Queue / 输入就绪队列");
    }
    if (auto *txtDqPush = this->findChild<QLabel*>("txtDqPush")) {
        txtDqPush->setText("Input Push / 入队速率:");
    }
    if (auto *txtDqDrop = this->findChild<QLabel*>("txtDqDrop")) {
        txtDqDrop->setText("Input Dropped / 丢帧:");
    }
    if (auto *txtVramOther = this->findChild<QLabel*>("txtVramOther")) {
        txtVramOther->setText("Residual / 未归因:");
    }

    m_titleBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_titleLabel->setObjectName("customTitleLabel");
    m_btnCloseDialog->setObjectName("titleCloseButton");

    QFont titleFont = m_titleLabel->font();
    titleFont.setPointSize(13);
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    const int titleLabelH = std::max(36, QFontMetrics(titleFont).height() + 10);
    m_titleLabel->setFixedHeight(titleLabelH);
    m_titleLabel->setContentsMargins(0, 0, 0, 0);
    m_titleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_titleLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    m_btnCloseDialog->setFixedSize(22, 22);
    QFont closeFont = m_btnCloseDialog->font();
    closeFont.setPointSize(11);
    closeFont.setBold(true);
    m_btnCloseDialog->setFont(closeFont);
    m_btnCloseDialog->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    const int titleBarH = std::max(titleLabelH + 16, 48);
    m_titleBar->setFixedHeight(titleBarH);

    connect(m_btnCloseDialog, &QPushButton::clicked, this, &QDialog::reject);
    m_titleBar->installEventFilter(this);
    m_titleLabel->installEventFilter(this);

    if (!m_resizeGrip) {
        m_resizeGrip = new QSizeGrip(this);
        m_resizeGrip->setFixedSize(14, 14);
        m_resizeGrip->setToolTip("Drag to resize");
        if (auto *bodyLayout = this->findChild<QVBoxLayout*>("verticalLayoutBody")) {
            auto *gripRow = new QHBoxLayout();
            gripRow->setContentsMargins(0, 0, 0, 0);
            gripRow->setSpacing(0);
            gripRow->addStretch();
            gripRow->addWidget(m_resizeGrip, 0, Qt::AlignRight | Qt::AlignBottom);
            bodyLayout->addLayout(gripRow);
        }
    }

    QString bigNum = "font-size:12px; font-weight:600; color:#334155;";
    m_lblDecodeFps->setStyleSheet(bigNum);
    m_lblInferFps->setStyleSheet(bigNum);
    m_lblDisplayFps->setStyleSheet(bigNum);
    m_lblDetections->setStyleSheet(bigNum);
    m_lblDqDrop->setStyleSheet(bigNum);
    m_lblDqPush->setStyleSheet(bigNum);
    m_lblInputArena->setStyleSheet("QLabel { font-family: 'Noto Sans Mono', 'DejaVu Sans Mono', monospace; font-size: 11px; color:#334155; }");
    m_lblOutputArena->setStyleSheet("QLabel { font-family: 'Noto Sans Mono', 'DejaVu Sans Mono', monospace; font-size: 11px; color:#334155; }");
    m_lblBottleneck->setStyleSheet("QLabel { font-weight: bold; padding: 4px; color: #DC2626; }");

    const QString dashboardTitleStyle =
        "QLabel {"
        " color: #1f3b47;"
        " font-size: 14px;"
        " font-weight: 800;"
        " letter-spacing: 0.4px;"
        " background: transparent;"
        " border: 1px solid #c4d7d0;"
        " border-radius: 7px;"
        " padding: 4px 8px;"
        "}";

    const QStringList dashboardTitleNames = {
        "titleGpuMem", "titleSlotPool", "titleVram",
        "titleThroughput", "titleWorker", "titleDetQueue",
        "titleBottleneck", "titleLoadTest"
    };
    QFont dashboardTitleFont;
    dashboardTitleFont.setPointSize(14);
    dashboardTitleFont.setBold(true);
    for (const auto &labelName : dashboardTitleNames) {
        if (auto *title = this->findChild<QLabel*>(labelName)) {
            title->setFont(dashboardTitleFont);
            title->setStyleSheet(dashboardTitleStyle);
            title->setMinimumHeight(24);
        }
    }

    if (auto *cardGpuMem = this->findChild<QWidget*>("cardGpuMem")) {
        cardGpuMem->setToolTip("当前 CUDA 设备的总显存与已用显存。值越高表示 GPU 负载越重。");
    }
    if (auto *cardVram = this->findChild<QWidget*>("cardVram")) {
        cardVram->setToolTip("显存闭环拆分：TensorArenas + TRT runtime + active contexts + decoder/display + unknown(used-known)。\nContext 同时显示 active 与 created，便于区分当前占用和历史累计。");
    }
    if (auto *cardSlotPool = this->findChild<QWidget*>("cardSlotPool")) {
        cardSlotPool->setToolTip("SlotPool 管理的推理对象池。每个 Slot 对应一次批处理任务的元数据容器。\nActive 越高表示并发推理更繁忙。");
    }
    if (auto *cardDetQueue = this->findChild<QWidget*>("cardDetQueue")) {
        cardDetQueue->setToolTip("Input Ready Queue（帧队列，不是 Slot 队列）：解码上传成功后进入 Ready，Worker 取走后进入 Inflight。\nReady 高=输入积压；Inflight 高=在推理流中等待/执行。 ");
    }
    if (auto *cardThroughput = this->findChild<QWidget*>("cardThroughput")) {
        cardThroughput->setToolTip("各阶段每秒处理量。数值为最近采样窗口平均值。");
    }
    if (auto *cardWorker = this->findChild<QWidget*>("cardWorker")) {
        cardWorker->setToolTip("Worker 线程效率诊断：Idle、批量利用率、Slot 等待、预处理耗时。");
    }
    if (auto *cardBottleneck = this->findChild<QWidget*>("cardBottleneck")) {
        cardBottleneck->setToolTip("自动检测管线瓶颈所在阶段。可在滚动区域查看完整分析文本。");
    }
    if (auto *cardLoadTest = this->findChild<QWidget*>("cardLoadTest")) {
        cardLoadTest->setToolTip("真实解码压测：使用真实通道解码+推理并可调目标 FPS。");
    }

    if (auto *dashPage = this->findChild<QWidget*>("dashPage")) {
        auto cards = dashPage->findChildren<QWidget*>();
        for (auto *b : cards) {
            if (!b->property("card").toBool()) continue;
            auto *eff = new QGraphicsDropShadowEffect(b);
            eff->setBlurRadius(18);
            eff->setOffset(0, 4);
            eff->setColor(QColor(0,0,0,36));
            b->setGraphicsEffect(eff);
        }
    }

    if (auto *colLayout = this->findChild<QHBoxLayout*>("dashColumnsLayout")) {
        colLayout->setContentsMargins(2, 2, 2, 2);
        colLayout->setSpacing(8);
        colLayout->setStretch(0, 1);
        colLayout->setStretch(1, 1);
        colLayout->setStretch(2, 1);
        colLayout->setSizeConstraint(QLayout::SetDefaultConstraint);
    }

    if (auto *outer = this->findChild<QVBoxLayout*>("dashOuterLayout")) {
        outer->setSpacing(6);
    }

    auto normalizeCol = [this](const char* layoutName) {
        if (auto *layout = this->findChild<QVBoxLayout*>(layoutName)) {
            layout->setContentsMargins(0, 0, 0, 0);
            layout->setSpacing(6);
        }
    };
    normalizeCol("leftColumnLayout");
    normalizeCol("middleColumnLayout");
    normalizeCol("rightColumnLayout");

    if (auto *leftCol = this->findChild<QVBoxLayout*>("leftColumnLayout")) {
        leftCol->setStretch(0, 0);
        leftCol->setStretch(1, 0);
        leftCol->setStretch(2, 1);
    }
    if (auto *midCol = this->findChild<QVBoxLayout*>("middleColumnLayout")) {
        midCol->setStretch(0, 0);
        midCol->setStretch(1, 0);
        midCol->setStretch(2, 1);
    }
    if (auto *rightCol = this->findChild<QVBoxLayout*>("rightColumnLayout")) {
        rightCol->setStretch(0, 0);
        rightCol->setStretch(1, 1);
    }

    if (auto *cardSlotPool = this->findChild<QWidget*>("cardSlotPool")) {
        cardSlotPool->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    }
    if (auto *cardArena = this->findChild<QWidget*>("cardArena")) {
        cardArena->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    }
    if (auto *cardWorker = this->findChild<QWidget*>("cardWorker")) {
        cardWorker->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    }
    if (auto *cardBottleneck = this->findChild<QWidget*>("cardBottleneck")) {
        cardBottleneck->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    }

    const QStringList compactCardLayouts = {
        "vlCardGpuMem", "vlCardSlotPool", "vlCardVram",
        "vlCardThroughput", "vlCardWorker", "vlCardDetQueue",
        "vlCardBottleneck", "vlCardLoadTest", "vlCardParams", "vlCardModel"
    };
    for (const auto &layoutName : compactCardLayouts) {
        if (auto *layout = this->findChild<QVBoxLayout*>(layoutName)) {
            layout->setContentsMargins(6, 6, 6, 6);
            layout->setSpacing(4);
        }
    }

    if (auto *cardLoadTest = this->findChild<QWidget*>("cardLoadTest")) {
        cardLoadTest->setMaximumHeight(110);
    }

    const Qt::TextInteractionFlags textFlags = Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard;
    const auto labels = this->findChildren<QLabel*>();
    for (QLabel* label : labels) {
        if (!label) continue;
        label->setTextInteractionFlags(textFlags);
    }

    const QList<QLabel*> wrapLabels = {
        m_lblVramSlot, m_lblVramCtx, m_lblVramDecoder, m_lblVramOther,
        m_lblInputArena, m_lblOutputArena, m_lblSlotPool, m_lblDetQueue,
        m_lblInferFps, m_lblDecodeFps, m_lblDisplayFps,
        m_lblWorkerIdle, m_lblBatchUtil, m_lblPeakSlots, m_lblCtxPool,
        m_lblInputArenaStates, m_lblOutputArenaStates, m_lblBottleneck
    };
    for (QLabel* label : wrapLabels) {
        if (!label) continue;
        label->setWordWrap(true);
        label->setMinimumWidth(0);
        label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    }

    // ---- 信号连接 ----
    connect(m_btnApply,    &QPushButton::clicked, this, &AdvancedSettingsDialog::onApply);
    connect(m_btnDefaults, &QPushButton::clicked, this, &AdvancedSettingsDialog::onRestoreDefaults);
    connect(m_btnLoadTest, &QPushButton::clicked, this, &AdvancedSettingsDialog::runLoadTest);
    connect(m_btnStopTest, &QPushButton::clicked, this, &AdvancedSettingsDialog::stopLoadTest);
    connect(m_btnCopyDash, &QPushButton::clicked, this, &AdvancedSettingsDialog::copyDashboard);

    connect(m_btnBrowseModel, &QPushButton::clicked, this, [this](){
        QString f = QFileDialog::getOpenFileName(this, "Select TensorRT Engine",
            m_editModelPath->text(), "Engine Files (*.engine *.trt);;All Files (*)");
        if (!f.isEmpty()) m_editModelPath->setText(f);
    });

    connect(m_btnBrowseClasses, &QPushButton::clicked, this, [this](){
        QString f = QFileDialog::getOpenFileName(this, "Select Classes File",
            m_editClassesPath->text(), "Text Files (*.txt);;All Files (*)");
        if (!f.isEmpty()) m_editClassesPath->setText(f);
    });
}

// ===================== 公共接口 ============================

AdvancedSettingsDialog::Settings AdvancedSettingsDialog::getSettings() const
{
    Settings s;
    s.baseSlots      = 4;
    s.inputArenaFrames = m_spinSlots->value();
    s.outputArenaFrames = m_spinContextPool->value();
    s.inferenceStreams = std::max(1, m_spinInferenceStreams->value());
    s.workerCount    = s.inferenceStreams;
    s.workerMaxBatch = m_spinBatch->value();
    s.contextPoolSize = 1;
    s.modelPath      = m_editModelPath->text();
    s.classesPath    = m_editClassesPath->text();
    s.statsInterval  = m_spinStatsInterval->value();
    s.displayConfThreshold = m_spinDisplayConf ? m_spinDisplayConf->value() : 0.55;
    return s;
}

void AdvancedSettingsDialog::setSettings(const Settings& s)
{
    m_spinSlots->setValue(std::max(16, s.inputArenaFrames));
    m_spinContextPool->setValue(std::max(16, s.outputArenaFrames));
    int parallel_workers = std::max(1, s.inferenceStreams);
    m_spinInferenceStreams->setValue(parallel_workers);
    m_spinWorkerCount->setValue(parallel_workers);
    m_spinBatch->setValue(s.workerMaxBatch);
    m_editModelPath->setText(s.modelPath);
    m_editClassesPath->setText(s.classesPath);
    m_spinStatsInterval->setValue(s.statsInterval);
    if (m_spinDisplayConf) {
        m_spinDisplayConf->setValue(std::clamp(s.displayConfThreshold, 0.01, 1.0));
    }
}

// ===================== 槽函数 ============================

void AdvancedSettingsDialog::onApply()
{
    Settings s = getSettings();
    saveToDisk(s);
    Q_EMIT settingsApplied(s);
}

void AdvancedSettingsDialog::onRestoreDefaults()
{
    setSettings(defaultSettings());
}

void AdvancedSettingsDialog::copyDashboard()
{
    if (m_lastDashboardText.isEmpty()) {
        refreshDashboard();
    }
    // 组合当前快照 + 最近历史
    QStringList parts;
    parts << "=== Current Dashboard ===";
    parts << m_lastDashboardText;
    if (!m_dashboardHistory.isEmpty()) {
        parts << "";
        parts << QString("=== Recent History (%1 snapshots) ===").arg(m_dashboardHistory.size());
        // 最新在前
        for (int i = m_dashboardHistory.size() - 1; i >= 0; --i)
            parts << m_dashboardHistory[i] << "";
    }
    QApplication::clipboard()->setText(parts.join('\n'));
    m_lblLoadTest->setText("Copied dashboard + history / 已复制（含历史）");
    QTimer::singleShot(1200, this, [this]() { refreshDashboard(); });
}

void AdvancedSettingsDialog::refreshDashboard()
{
    size_t used_bytes = 0;
    size_t total_bytes = 0;
    bool has_gpu_mem = false;
    size_t input_inflight_frames_snapshot = 0;
    size_t output_active_frames_snapshot = 0;
    size_t output_pending_frames_snapshot = 0;
    // --- GPU Memory ---
    {
        size_t free_bytes = 0;
        cudaError_t err = cudaMemGetInfo(&free_bytes, &total_bytes);
        if (err == cudaSuccess && total_bytes > 0) {
            used_bytes = total_bytes - free_bytes;
            double usedMiB  = used_bytes / (1024.0 * 1024.0);
            double totalMiB = total_bytes / (1024.0 * 1024.0);
            int pct = static_cast<int>(100.0 * used_bytes / total_bytes);
            m_barGpuMem->setValue(pct);
            m_lblGpuMem->setText(QString("%1 / %2 MiB (%3%)")
                .arg(usedMiB, 0, 'f', 1).arg(totalMiB, 0, 'f', 1).arg(pct));
            has_gpu_mem = true;
        } else {
            m_barGpuMem->setValue(0);
            m_lblGpuMem->setText("N/A");
        }
    }

    // --- VRAM Breakdown (estimate) ---
    {
        const double mib = 1024.0 * 1024.0;
        auto& tensorMgr = TensorArenaManager::getInstance();
        auto inArena = tensorMgr.inputStats();
        auto outArena = tensorMgr.outputStats();
        size_t slot_bytes = inArena.total_bytes + outArena.total_bytes;
        auto& detector = TRTDetector::getInstance();
        int ctx_created = detector.getContextTotalCount();
        int ctx_alive = detector.getContextActiveCount();
        size_t ctx_bytes = detector.getContextActiveBytes();
        size_t trt_runtime_bytes = detector.getTrtRuntimeBytes();
        size_t decoder_bytes = VideoDecoder::totalDecoderVramBytes();
        size_t decode_frame_bytes = VideoDecoder::totalStandaloneFrameVramBytes();
        size_t display_bytes = DisplayWorker::totalDisplayVramBytes();

        if (!has_gpu_mem || total_bytes == 0) {
            m_lblVramSlot->setText("--");
            m_lblVramCtx->setText("--");
            m_lblVramDecoder->setText("--");
            m_lblVramOther->setText("--");
            m_lblDecoderCount->setText("--");
        } else {
            double slotMiB = slot_bytes / mib;
            double ctxMiB = ctx_bytes / mib;
            double trtRuntimeMiB = trt_runtime_bytes / mib;
            double decCtxMiB = decoder_bytes / mib;
            double decFrameMiB = decode_frame_bytes / mib;
            double displayMiB = display_bytes / mib;
            const int ctx_avg_mib = (ctx_alive > 0)
                ? static_cast<int>(std::llround((ctx_bytes / mib) / static_cast<double>(ctx_alive)))
                : 0;
            size_t known = slot_bytes + trt_runtime_bytes + ctx_bytes + decoder_bytes + decode_frame_bytes + display_bytes;
            double knownMiB = static_cast<double>(known) / mib;
            double unknownMiB = (static_cast<double>(used_bytes) - static_cast<double>(known)) / mib;

            m_lblVramSlot->setText(QString("%1 MiB (input+output arenas)").arg(slotMiB, 0, 'f', 1));
            m_lblVramCtx->setText(QString("%1 MiB (active=%2, created=%3, avg=%4 MiB/ctx)")
                                  .arg(ctxMiB, 0, 'f', 1)
                                  .arg(ctx_alive)
                                  .arg(ctx_created)
                                  .arg(ctx_avg_mib));
            m_lblVramDecoder->setText(QString("%1 MiB (TRT runtime) + %2 MiB (decoder) + %3 MiB (frame copy) + %4 MiB (display)")
                                      .arg(trtRuntimeMiB, 0, 'f', 1)
                                      .arg(decCtxMiB, 0, 'f', 1)
                                      .arg(decFrameMiB, 0, 'f', 1)
                                      .arg(displayMiB, 0, 'f', 1));
            m_lblVramOther->setText(QString("%1 MiB (residual=used-known, known=%2 MiB)")
                                    .arg(unknownMiB, 0, 'f', 1)
                                    .arg(knownMiB, 0, 'f', 1));
            m_lblDecoderCount->setText(QString("%1 / %2 (max %3)")
                .arg(VideoDecoder::hwDecoderCount())
                .arg(VideoDecoder::swDecoderCount())
                .arg(VideoDecoder::maxHwDecoders()));
        }
    }

    // --- GPU Arenas ---
    {
        auto& tensorMgr = TensorArenaManager::getInstance();
        auto inputStats = tensorMgr.inputStats();
        auto outputStats = tensorMgr.outputStats();
        auto inputSegments = tensorMgr.inputSegments();
        auto outputSegments = tensorMgr.outputSegments();
        uintptr_t inputBase = tensorMgr.inputBaseAddress();

        auto storeStats = InputFrameArenaStore::getInstance().getStats();
        auto sampleRanges = InputFrameArenaStore::getInstance().getSampleRanges();

        std::vector<std::pair<size_t, size_t>> readyRanges;
        std::vector<std::pair<size_t, size_t>> inflightRanges;
        readyRanges.reserve(sampleRanges.size());
        inflightRanges.reserve(sampleRanges.size());
        for (const auto& r : sampleRanges) {
            if (r.ptr < inputBase || r.bytes == 0) {
                continue;
            }
            size_t begin = static_cast<size_t>(r.ptr - inputBase);
            size_t end = begin + r.bytes;
            if (r.state == InputFrameArenaStore::SampleState::Ready) {
                readyRanges.emplace_back(begin, end);
            } else {
                inflightRanges.emplace_back(begin, end);
            }
        }

        auto overlaps = [](const std::vector<std::pair<size_t, size_t>>& ranges, size_t begin, size_t end) {
            for (const auto& range : ranges) {
                if (range.second <= begin || range.first >= end) {
                    continue;
                }
                return true;
            }
            return false;
        };

        auto toRatio = [](size_t v, size_t total) -> double {
            if (total == 0) return 0.0;
            return std::clamp(static_cast<double>(v) / static_cast<double>(total), 0.0, 1.0);
        };

        std::vector<ArenaStateBar::Segment> inputVisualSegs;
        inputVisualSegs.reserve(inputSegments.size());
        for (const auto& seg : inputSegments) {
            if (inputStats.total_bytes == 0 || seg.bytes == 0) continue;
            ArenaStateBar::Segment vis;
            vis.begin = toRatio(seg.offset, inputStats.total_bytes);
            vis.end = toRatio(seg.offset + seg.bytes, inputStats.total_bytes);
            if (seg.status == GpuArena::SegmentStatus::Free) {
                vis.color = QColor("#CBD5E1");
            } else {
                const size_t segBegin = seg.offset;
                const size_t segEnd = seg.offset + seg.bytes;
                if (overlaps(readyRanges, segBegin, segEnd)) {
                    vis.color = QColor("#22C55E");
                } else if (overlaps(inflightRanges, segBegin, segEnd)) {
                    vis.color = QColor("#F59E0B");
                } else {
                    vis.color = QColor("#22C55E");
                }
            }
            inputVisualSegs.push_back(vis);
        }
        if (m_barInputArenaState) {
            m_barInputArenaState->setSegments(inputVisualSegs);
        }

        std::vector<ArenaStateBar::Segment> outputVisualSegs;
        outputVisualSegs.reserve(outputSegments.size());
        for (const auto& seg : outputSegments) {
            if (outputStats.total_bytes == 0 || seg.bytes == 0) continue;
            ArenaStateBar::Segment vis;
            vis.begin = toRatio(seg.offset, outputStats.total_bytes);
            vis.end = toRatio(seg.offset + seg.bytes, outputStats.total_bytes);
            switch (seg.status) {
                case GpuArena::SegmentStatus::Free: vis.color = QColor("#CBD5E1"); break;
                case GpuArena::SegmentStatus::PendingFree: vis.color = QColor("#F59E0B"); break;
                case GpuArena::SegmentStatus::Active: vis.color = QColor("#3B82F6"); break;
            }
            outputVisualSegs.push_back(vis);
        }
        if (m_barOutputArenaState) {
            m_barOutputArenaState->setSegments(outputVisualSegs);
        }

        size_t input_total_frames = static_cast<size_t>(std::max(16, m_spinSlots ? m_spinSlots->value() : 16));
        size_t input_ready_frames = storeStats.ready_frames;
        size_t input_inflight_frames = storeStats.inflight_frames;
        size_t input_free_frames = (input_total_frames > (input_ready_frames + input_inflight_frames))
            ? (input_total_frames - input_ready_frames - input_inflight_frames)
            : 0;

        const size_t output_bytes_per_frame = TRTDetector::getInstance().getOutputBytesPerFrame();
        size_t output_total_frames = static_cast<size_t>(std::max(16, m_spinContextPool ? m_spinContextPool->value() : 16));
        size_t output_active_frames = 0;
        size_t output_pending_frames = 0;
        if (output_bytes_per_frame > 0) {
            output_total_frames = outputStats.total_bytes / output_bytes_per_frame;
            output_active_frames = outputStats.active_alloc_bytes / output_bytes_per_frame;
            output_pending_frames = outputStats.pending_free_bytes / output_bytes_per_frame;
        }
        size_t output_free_frames = 0;
        if (output_total_frames > output_active_frames + output_pending_frames) {
            output_free_frames = output_total_frames - output_active_frames - output_pending_frames;
        }

        double mib = 1024.0 * 1024.0;
        QString inputText = QString("%1 / %2 MiB | frag %3% | frames %4/%5")
            .arg(inputStats.used_bytes / mib, 0, 'f', 1)
            .arg(inputStats.total_bytes / mib, 0, 'f', 1)
            .arg(inputStats.fragmentation_ratio * 100.0, 0, 'f', 1)
            .arg(input_ready_frames + input_inflight_frames)
            .arg(input_total_frames);
        QString outputText = QString("%1 / %2 MiB | frag %3% | frames %4/%5")
            .arg(outputStats.used_bytes / mib, 0, 'f', 1)
            .arg(outputStats.total_bytes / mib, 0, 'f', 1)
            .arg(outputStats.fragmentation_ratio * 100.0, 0, 'f', 1)
            .arg(output_active_frames + output_pending_frames)
            .arg(output_total_frames);
        m_lblInputArena->setText(inputText);
        m_lblOutputArena->setText(outputText);

        input_inflight_frames_snapshot = input_inflight_frames;
        output_active_frames_snapshot = output_active_frames;
        output_pending_frames_snapshot = output_pending_frames;

        auto pctText = [](size_t v, size_t total) {
            if (total == 0) return QString("0.0");
            return QString::number(100.0 * static_cast<double>(v) / static_cast<double>(total), 'f', 1);
        };
        if (m_lblInputArenaStates) {
            m_lblInputArenaStates->setText(
                QString("⬜ Free %1 (%2%)  "
                        "🟩 Ready %3 (%4%)  "
                        "🟧 Inflight %5 (%6%)")
                    .arg(input_free_frames)
                    .arg(pctText(input_free_frames, input_total_frames))
                    .arg(input_ready_frames)
                    .arg(pctText(input_ready_frames, input_total_frames))
                    .arg(input_inflight_frames)
                    .arg(pctText(input_inflight_frames, input_total_frames))
            );
        }
        if (m_lblOutputArenaStates) {
            m_lblOutputArenaStates->setText(
                QString("⬜ Free %1 (%2%)  "
                        "🟦 Active %3 (%4%)  "
                        "🟧 Pending %5 (%6%)")
                    .arg(output_free_frames)
                    .arg(pctText(output_free_frames, output_total_frames))
                    .arg(output_active_frames)
                    .arg(pctText(output_active_frames, output_total_frames))
                    .arg(output_pending_frames)
                    .arg(pctText(output_pending_frames, output_total_frames))
            );
        }
    }

    // --- Slot Pool ---
    {
        int avail = static_cast<int>(SlotPool::getInstance().freeSlots());
        int total = static_cast<int>(SlotPool::getInstance().totalSlots());
        int active = static_cast<int>(SlotPool::getInstance().activeSlots());
        if (total > 0) {
            int pct = 100 * active / total;
            m_barSlotPool->setValue(pct);
            int workers = std::max(1, m_spinInferenceStreams ? m_spinInferenceStreams->value() : 1);
            m_lblSlotPool->setText(QString("Active %1 / %2  (Free %3, workers=%4, estEvents=%5, inputInflightFrames=%6)")
                                   .arg(active).arg(total).arg(avail)
                                   .arg(workers)
                                   .arg(output_active_frames_snapshot + output_pending_frames_snapshot)
                                   .arg(input_inflight_frames_snapshot));
        } else {
            m_barSlotPool->setValue(0);
            m_lblSlotPool->setText("Not initialized");
        }
    }

    // --- Input Ready Queue ---
    {
        auto storeStats = InputFrameArenaStore::getInstance().getStats();
        size_t sz  = storeStats.ready_frames;
        size_t cap = storeStats.max_ready_frames;
        if (cap > 0) {
            int pct = static_cast<int>(100 * sz / cap);
            m_barDetQueue->setValue(pct);
            m_lblDetQueue->setText(QString("ReadyFrames %1 / %2  (InflightFrames %3, Dropped %4)")
                                       .arg(sz).arg(cap)
                                       .arg(storeStats.inflight_frames)
                                       .arg(storeStats.dropped_frames));
        } else {
            m_barDetQueue->setValue(0);
            m_lblDetQueue->setText("Not initialized");
        }
    }

    // --- Throughput (swap-and-read) ---
    auto& ps = PipelineStats::getInstance();
    uint64_t decoded   = ps.frames_decoded.exchange(0, std::memory_order_relaxed);
    uint64_t inferred  = ps.frames_inferred.exchange(0, std::memory_order_relaxed);
    uint64_t displayed = ps.frames_displayed.exchange(0, std::memory_order_relaxed);
    uint64_t dets      = ps.detections_total.exchange(0, std::memory_order_relaxed);
    uint64_t dropped   = ps.frames_dropped_dq.exchange(0, std::memory_order_relaxed);
    uint64_t pushed    = ps.frames_pushed_dq.exchange(0, std::memory_order_relaxed);
    uint64_t batches   = ps.batches_inferred.exchange(0, std::memory_order_relaxed);
    uint64_t demux_pkts = ps.demux_packets_read.exchange(0, std::memory_order_relaxed);
    uint64_t demux_read_us = ps.demux_read_us.exchange(0, std::memory_order_relaxed);
    uint64_t decode_pop_wait_us = ps.decode_pop_wait_us.exchange(0, std::memory_order_relaxed);
    uint64_t decode_send_us = ps.decode_send_us.exchange(0, std::memory_order_relaxed);
    uint64_t decode_receive_us = ps.decode_receive_us.exchange(0, std::memory_order_relaxed);
    uint64_t decode_upload_us = ps.decode_upload_us.exchange(0, std::memory_order_relaxed);
    uint64_t frames_uploaded = ps.frames_uploaded.exchange(0, std::memory_order_relaxed);
    uint64_t framequeue_push_wait_us = ps.framequeue_push_wait_us.exchange(0, std::memory_order_relaxed);
    uint64_t frames_pushed_fq = ps.frames_pushed_fq.exchange(0, std::memory_order_relaxed);

    // Worker efficiency counters
    uint64_t w_pop_empty  = ps.worker_pop_empty.exchange(0, std::memory_order_relaxed);
    uint64_t w_batches    = ps.worker_batches_popped.exchange(0, std::memory_order_relaxed);
    uint64_t w_frames     = ps.worker_frames_popped.exchange(0, std::memory_order_relaxed);
    uint64_t w_slot_us    = ps.worker_slot_wait_us.exchange(0, std::memory_order_relaxed);
    uint64_t w_preproc_us = ps.worker_preproc_us.exchange(0, std::memory_order_relaxed);
    uint64_t w_gpu_preproc_us = ps.worker_gpu_preproc_us.exchange(0, std::memory_order_relaxed);
    uint64_t w_gpu_infer_us = ps.worker_gpu_infer_us.exchange(0, std::memory_order_relaxed);
    uint64_t w_gpu_batches = ps.worker_gpu_batches.exchange(0, std::memory_order_relaxed);
    uint64_t w_no_slot = ps.worker_no_slot.exchange(0, std::memory_order_relaxed);
    uint64_t w_no_stream = ps.worker_no_stream.exchange(0, std::memory_order_relaxed);
    uint64_t w_alloc_in_fail = ps.worker_alloc_input_fail.exchange(0, std::memory_order_relaxed);
    uint64_t w_alloc_out_fail = ps.worker_alloc_output_fail.exchange(0, std::memory_order_relaxed);
    uint64_t w_submit_fail = ps.worker_submit_fail.exchange(0, std::memory_order_relaxed);
    uint64_t post_d2h_us = ps.postprocess_d2h_us.exchange(0, std::memory_order_relaxed);
    uint64_t post_batches = ps.postprocess_batches.exchange(0, std::memory_order_relaxed);
    uint64_t post_frames = ps.postprocess_frames.exchange(0, std::memory_order_relaxed);

    double interval = m_dashTimer->interval() / 1000.0;
    if (interval <= 0) interval = 2.0;

    auto per_item_ms = [](uint64_t total_us, uint64_t count) -> double {
        if (count == 0) return 0.0;
        return (static_cast<double>(total_us) / 1000.0) / static_cast<double>(count);
    };
    double decode_fps = decoded / interval;
    double infer_fps  = inferred / interval;
    double display_fps = displayed / interval;
    double avg_batch = (w_batches > 0) ? static_cast<double>(w_frames) / static_cast<double>(w_batches) : 0.0;
    double avg_gpu_batch = (w_gpu_batches > 0) ? static_cast<double>(inferred) / static_cast<double>(w_gpu_batches) : avg_batch;
    if (avg_gpu_batch <= 0.0) avg_gpu_batch = 1.0;
    double avg_post_batch = (post_batches > 0) ? static_cast<double>(post_frames) / static_cast<double>(post_batches) : avg_gpu_batch;
    if (avg_post_batch <= 0.0) avg_post_batch = avg_gpu_batch;

    double demux_ms = per_item_ms(demux_read_us, demux_pkts);
    double decode_send_ms = per_item_ms(decode_send_us, std::max<uint64_t>(1, demux_pkts));
    double decode_recv_ms = per_item_ms(decode_receive_us, decoded);
    double decode_pop_wait_ms = per_item_ms(decode_pop_wait_us, std::max<uint64_t>(1, demux_pkts));
    double htod_ms = per_item_ms(decode_upload_us, frames_uploaded);
    double fq_push_wait_ms = per_item_ms(framequeue_push_wait_us, frames_pushed_fq);
    double preproc_cpu_ms = per_item_ms(w_preproc_us, w_batches);
    double preproc_gpu_ms = per_item_ms(w_gpu_preproc_us, w_gpu_batches);
    double infer_gpu_ms = per_item_ms(w_gpu_infer_us, w_gpu_batches);
    double slot_wait_ms = per_item_ms(w_slot_us, w_batches);
    double d2h_ms = per_item_ms(post_d2h_us, post_batches);

    // Throughput labels (instant + short-window EMA)
    {
        const double alpha = 0.35;
        if (!m_fpsEmaInited) {
            m_decodeFpsEma = decode_fps;
            m_inferFpsEma = infer_fps;
            m_displayFpsEma = display_fps;
            m_fpsEmaInited = true;
        } else {
            m_decodeFpsEma = alpha * decode_fps + (1.0 - alpha) * m_decodeFpsEma;
            m_inferFpsEma = alpha * infer_fps + (1.0 - alpha) * m_inferFpsEma;
            m_displayFpsEma = alpha * display_fps + (1.0 - alpha) * m_displayFpsEma;
        }
    }

    m_lblDecodeFps->setText(QString("%1 fps (avg %2)")
        .arg(QString::number(decode_fps, 'f', 1))
        .arg(QString::number(m_decodeFpsEma, 'f', 1)));
    m_lblInferFps->setText(QString("%1 fps (avg %2, %3 batches/s)")
        .arg(QString::number(infer_fps, 'f', 1))
        .arg(QString::number(m_inferFpsEma, 'f', 1))
        .arg(QString::number(batches / interval, 'f', 1)));
    m_lblDisplayFps->setText(QString("%1 fps (avg %2)")
        .arg(QString::number(display_fps, 'f', 1))
        .arg(QString::number(m_displayFpsEma, 'f', 1)));
    m_lblDetections->setText( QString::number(dets      / interval, 'f', 1) + " /s");
    m_lblDqPush->setText(     QString::number(pushed    / interval, 'f', 1) + " fps");
    m_lblDqDrop->setText(     QString::number(dropped   / interval, 'f', 1) + " /s");


    // --- Worker Efficiency ---
    {
        uint64_t w_total = w_pop_empty + w_batches;
        double idle_pct = (w_total > 0) ? 100.0 * w_pop_empty / w_total : 0.0;
        QString idleColor = (idle_pct > 60) ? "#DC2626" : (idle_pct > 30) ? "#D97706" : "#16A34A";
        m_lblWorkerIdle->setStyleSheet(QString("QLabel { color: %1; font-weight: 700; }").arg(idleColor));
        m_lblWorkerIdle->setText(QString("%1 / %2 cycles (%3%)")
            .arg(w_pop_empty).arg(w_total)
            .arg(idle_pct, 0, 'f', 1));

        m_lblBatchUtil->setText(QString("%1 frames/pop (of max batch)")
            .arg(avg_batch, 0, 'f', 1));

        double avg_slot_ms = (w_batches > 0) ? (w_slot_us / 1000.0) / w_batches : 0.0;
        m_lblSlotWait->setText(QString("%1 ms avg/batch").arg(avg_slot_ms, 0, 'f', 2));

        double avg_preproc_ms = (w_batches > 0) ? (w_preproc_us / 1000.0) / w_batches : 0.0;
        m_lblPreprocTime->setText(QString("%1 ms avg/batch").arg(avg_preproc_ms, 0, 'f', 2));

        size_t activeSlots = SlotPool::getInstance().activeSlots();
        size_t totalSlots = SlotPool::getInstance().totalSlots();
        size_t peakSlotsWindow = SlotPool::getInstance().peakActiveSlotsAndReset();
        if (peakSlotsWindow == 0) {
            peakSlotsWindow = activeSlots;
        }
        m_lblPeakSlots->setText(QString("%1 / %2 (now %3)")
            .arg(peakSlotsWindow)
            .arg(totalSlots)
            .arg(activeSlots));
        m_lblCtxPool->setText(QString("no_slot=%1, no_stream=%2, alloc_in=%3, alloc_out=%4, submit=%5")
            .arg(w_no_slot)
            .arg(w_no_stream)
            .arg(w_alloc_in_fail)
            .arg(w_alloc_out_fail)
            .arg(w_submit_fail));

    }

    // --- Bottleneck Analysis ---
    {
        uint64_t w_total  = w_pop_empty + w_batches;
        double idle_pct   = (w_total > 0) ? 100.0 * w_pop_empty / w_total : 0.0;
        double avg_slot_ms = slot_wait_ms;
        double input_fps = decode_fps;
        double infer_ratio = (input_fps > 1e-6) ? (infer_fps / input_fps) : 1.0;
        double dq_drop_ps = dropped / interval;
        double max_batch_cfg = std::max(1.0, static_cast<double>(m_spinBatch ? m_spinBatch->value() : 1));
        double batch_util = avg_batch / max_batch_cfg;
        uint64_t fail_total = w_no_slot + w_no_stream + w_alloc_in_fail + w_alloc_out_fail + w_submit_fail;
        auto storeStats = InputFrameArenaStore::getInstance().getStats();
        size_t dq_sz = storeStats.ready_frames;
        size_t dq_cap = storeStats.max_ready_frames;

        QString analysis;
        if (decode_fps < 1.0 && infer_fps < 1.0) {
            analysis = "⏸ Idle — 无活跃通道 / No active channels";
        } else if (infer_fps < 1.0 && fail_total > 0) {
            analysis = QString("🔴 WORKER-FAILURE / 推理任务失败\n"
                "fail(total=%1): no_slot=%2, no_stream=%3, alloc_in=%4, alloc_out=%5, submit=%6。\n"
                "建议: 优先检查 Arena 容量（input/output frames）与 TRT 提交路径。")
                .arg(fail_total).arg(w_no_slot).arg(w_no_stream).arg(w_alloc_in_fail).arg(w_alloc_out_fail).arg(w_submit_fail);
        } else if (infer_ratio < 0.92 && (dq_drop_ps > 1.0 || dq_sz > dq_cap * 0.2 || batch_util < 0.45)) {
            analysis = QString("🔴 INFERENCE-LIMITED / 推理并行不足\n"
                "Input=%1 fps, Infer=%2 fps, Gap=%3 fps, AvgBatch=%4/%5。\n"
                "建议: 提高并行 Worker 或提升微批聚合利用率。")
                .arg(input_fps, 0, 'f', 1)
                .arg(infer_fps, 0, 'f', 1)
                .arg(std::max(0.0, input_fps - infer_fps), 0, 'f', 1)
                .arg(avg_batch, 0, 'f', 1)
                .arg(max_batch_cfg, 0, 'f', 0);
        } else if (idle_pct > 65.0 && infer_ratio >= 0.92) {
            analysis = QString("🟡 INPUT-LIMITED / 输入供给不足\n"
                "Workers 空转 %1%%，当前输入速率本身偏低。\n"
                "建议: 增加活跃通道或提高源输入速率。")
                .arg(idle_pct, 0, 'f', 1);
        } else if (avg_slot_ms > 5.0) {
            analysis = QString("🟡 SLOT-LIMITED / 显存槽瓶颈\n"
                "Slot 等待 %1ms/batch。\n"
                "建议: 增加 SlotPool 数量或降低并行/批量。").arg(avg_slot_ms, 0, 'f', 1);
        } else {
            analysis = "🟢 BALANCED / 均衡\n管线各阶段匹配良好。";
        }
        m_lblBottleneck->setText(analysis);
    }

    // 生成可复制的文本报告，方便一键复制到剪贴板
    {
        auto plain = [](const QString& t) { return toPlainReportText(t); };
        QStringList lines;
        lines << "Pipeline Diagnostic Report";
        lines << "--------------------------";
        lines << QString("GPU Memory: %1").arg(plain(m_lblGpuMem->text()));
        lines << QString("VRAM Tensor Arenas (est): %1").arg(plain(m_lblVramSlot->text()));
        lines << QString("VRAM Context (est): %1").arg(plain(m_lblVramCtx->text()));
        lines << QString("VRAM Decoder (est): %1").arg(plain(m_lblVramDecoder->text()));
        lines << QString("VRAM Residual (est): %1").arg(plain(m_lblVramOther->text()));
        lines << "VRAM Formula: residual = used - (tensor_arenas + context_active + trt_runtime + decoder + frame_copy + display)";
        lines << QString("Input Arena: %1").arg(plain(m_lblInputArena->text()));
        if (m_lblInputArenaStates) lines << QString("Input Arena States: %1").arg(plain(m_lblInputArenaStates->text()));
        lines << QString("Output Arena: %1").arg(plain(m_lblOutputArena->text()));
        if (m_lblOutputArenaStates) lines << QString("Output Arena States: %1").arg(plain(m_lblOutputArenaStates->text()));
        lines << QString("Slot Pool: %1").arg(plain(m_lblSlotPool->text()));
        lines << QString("Input ReadyQ: %1").arg(plain(m_lblDetQueue->text()));
        // FrameQueues: 简单列出 ch sizes
        // 若需更详细可扩展
        lines << QString("Workers / Infer: %1, %2").arg(plain(m_lblInferFps->text())).arg(plain(m_lblBatchUtil->text()));
        lines << "";
        lines << "Throughput:";
        lines << QString("  Input: %1").arg(plain(m_lblDecodeFps->text()));
        lines << QString("  Infer:  %1").arg(plain(m_lblInferFps->text()));
        lines << QString("  Display: %1").arg(plain(m_lblDisplayFps->text()));
        lines << QString("  Detections: %1").arg(plain(m_lblDetections->text()));
        lines << QString("  Input Push / Dropped: %1 / %2").arg(plain(m_lblDqPush->text())).arg(plain(m_lblDqDrop->text()));
        lines << "";
        lines << "Worker Efficiency:";
        lines << QString("  Idle: %1").arg(plain(m_lblWorkerIdle->text()));
        lines << QString("  Avg batch: %1").arg(plain(m_lblBatchUtil->text()));
        lines << QString("  Slot wait: %1").arg(plain(m_lblSlotWait->text()));
        lines << QString("  Preproc: %1").arg(plain(m_lblPreprocTime->text()));
        lines << QString("  Peak Slots: %1").arg(plain(m_lblPeakSlots->text()));
        lines << QString("  Failures: %1").arg(plain(m_lblCtxPool->text()));
        lines << "";
        lines << "Stage Timings (avg ms):";
        lines << QString("  demux read: %1").arg(demux_ms, 0, 'f', 3);
        lines << QString("  decode send/recv: %1 / %2").arg(decode_send_ms, 0, 'f', 3).arg(decode_recv_ms, 0, 'f', 3);
        lines << QString("  decode pop wait: %1").arg(decode_pop_wait_ms, 0, 'f', 3);
        lines << QString("  HtoD upload: %1").arg(htod_ms, 0, 'f', 3);
        lines << QString("  preproc CPU/GPU: %1 / %2").arg(preproc_cpu_ms, 0, 'f', 3).arg(preproc_gpu_ms, 0, 'f', 3);
        lines << QString("  infer GPU (batch): %1").arg(infer_gpu_ms, 0, 'f', 3);
        lines << QString("  DtoH postprocess: %1").arg(d2h_ms, 0, 'f', 3);
        lines << QString("  slot/fq wait: %1 / %2").arg(slot_wait_ms, 0, 'f', 3).arg(fq_push_wait_ms, 0, 'f', 3);
        lines << "";
        lines << "Bottleneck:";
        lines << plain(m_lblBottleneck->text());

        m_lastDashboardText = lines.join('\n');

        // 追加到历史（保留最近 20 条快照）
        {
            QString ts = QDateTime::currentDateTime().toString("hh:mm:ss");
            m_dashboardHistory.append(QString("[%1]\n%2").arg(ts).arg(m_lastDashboardText));
            while (m_dashboardHistory.size() > 20) m_dashboardHistory.removeFirst();
        }
    }
}

void AdvancedSettingsDialog::runLoadTest()
{
    const bool useRealDecode = true;
    const int layoutMode = m_comboLoadLayout ? (m_comboLoadLayout->currentIndex() + 1) : 1;
    const int numChannels = (layoutMode == 1 ? 1 : (layoutMode == 2 ? 4 : 9));
    const int targetFps = m_spinLoadFps->value();
    const int durationSec = m_spinLoadDuration->value();

    m_loadTestRunning.store(true);
    m_btnLoadTest->setEnabled(false);
    m_btnStopTest->setEnabled(true);
    if (m_comboLoadLayout) m_comboLoadLayout->setEnabled(false);
    m_spinLoadFps->setEnabled(false);
    m_spinLoadDuration->setEnabled(false);

    Q_EMIT loadTestStartRequested(useRealDecode, numChannels, targetFps, layoutMode);
    m_lblLoadTest->setText("⏳ Preparing real decode...");

    PipelineStats::getInstance().resetAll();

    std::thread([this, numChannels, targetFps, durationSec]() {
        auto startTime = std::chrono::steady_clock::now();
        uint64_t lastDecoded = PipelineStats::getInstance().frames_decoded.load(std::memory_order_relaxed);

        while (m_loadTestRunning.load()) {
            auto now = std::chrono::steady_clock::now();
            double elapsed = std::chrono::duration<double>(now - startTime).count();
            if (elapsed >= durationSec) break;

            uint64_t curDecoded = PipelineStats::getInstance().frames_decoded.load(std::memory_order_relaxed);
            uint64_t delta = 0;
            if (curDecoded >= lastDecoded) delta = curDecoded - lastDecoded;
            lastDecoded = curDecoded;

            double inputFps = static_cast<double>(delta) / 0.5;
            int remain = static_cast<int>(durationSec - elapsed);

            QMetaObject::invokeMethod(this, [this, numChannels, targetFps, remain, inputFps]() {
                m_lblLoadTest->setText(QString("🔥 REAL %1ch×%2fps (in: %3fps) | %4s left")
                    .arg(numChannels).arg(targetFps).arg(inputFps, 0, 'f', 1).arg(remain));
            }, Qt::QueuedConnection);

            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        m_loadTestRunning.store(false);
        QMetaObject::invokeMethod(this, [this, numChannels, targetFps, durationSec]() {
            m_lblLoadTest->setText(QString("✅ Done [REAL]: %1ch×%2fps | %3s")
                .arg(numChannels).arg(targetFps).arg(durationSec));
            m_btnLoadTest->setEnabled(true);
            m_btnStopTest->setEnabled(false);
            if (m_comboLoadLayout) m_comboLoadLayout->setEnabled(true);
            m_spinLoadFps->setEnabled(true);
            m_spinLoadDuration->setEnabled(true);
            Q_EMIT loadTestStopRequested();
        }, Qt::QueuedConnection);
    }).detach();
}

void AdvancedSettingsDialog::stopLoadTest()
{
    m_loadTestRunning.store(false);
    m_btnStopTest->setEnabled(false);
    m_lblLoadTest->setText("⏹ Stopping... / 正在停止...");
}
