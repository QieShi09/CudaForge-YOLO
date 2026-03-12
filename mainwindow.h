#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMap>
#include <QThread>
#include <QVector>
#include <QSet>
#include <QDateTime>
#include <QPoint>
#include <nvtx3/nvToolsExt.h>
#include "VideoDecoder.hpp"
#include "DisplayManager.hpp"
#include "FrameQueue.hpp"
#include "src/core/Worker.hpp"
#include <vector>
#include <memory>
#include "src/widgets/AdvancedSettingsDialog.hpp"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class VideoWidget;
class QWidget;
class QLabel;
class QPushButton;

struct ChannelSettings {
    int sourceTypeIndex = 0; // 0: File, 1: RTSP, 2: Camera, 3: Image
    QString sourcePath;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private Q_SLOTS:
    void on_comboBox_layout_currentIndexChanged(int index);
    void on_btn_browse_clicked();
    void on_comboBox_sourceType_currentIndexChanged(int index);
    void on_comboBox_channelSelect_currentIndexChanged(int index);
    void on_lineEdit_source_textChanged(const QString &arg1);
    void on_btn_back_to_grid_clicked();
    void on_btn_pause_toggled(bool checked);
    void on_btn_snapshot_clicked();
    void on_btn_start_clicked();
    void on_btn_stop_clicked();
    void onChannelCloseRequested(int index);
    void on_comboBox_speed_currentIndexChanged(int index);
    void on_slider_seek_sliderReleased();
    void onPlaybackFinished(int channel_id);
    void onVideoReplayRequested(int channel_id);

private:
    Ui::MainWindow *ui;
    
    // 辅助函数
    void updateVideoGrid(int rows, int cols);
    void performSeekFromSlider();
    template<typename Func> void forEachVideoWidget(Func callback);
    VideoWidget* findVideoWidget(int channelId);
    void cleanupAnalysisDecoder();
    void loadChannelSettings(int channelIndex);
    void enterAnalysisMode(int channelIndex);
    void exitAnalysisMode();
    void logSystemMessage(const QString &msg, int level = 0);
    void startChannel(int channel_id);
    void stopChannel(int channel_id);

    void startDetection();
    void stopDetection(bool clear_results = true);
    void generateReport();
    void printPipelineStats();  // 定期输出管道性能统计
    void openAdvancedSettings();       // 打开高级设置弹窗
    void applySettings(const AdvancedSettingsDialog::Settings& s); // 应用设置
    int activeConfiguredChannels() const;
    int recommendedSlotCount(int effectiveBatch) const;
    
    // 通道设置
    QMap<int, ChannelSettings> m_channelSettings;
    int m_currentChannelIndex = 0;
    bool m_isAnalysisMode = false;

    // 视频管理
    DisplayManager* m_displayManager = nullptr;
    QMap<int, VideoDecoder*> m_decoders;
    QMap<int, QThread*> m_threads;
    QMap<int, FrameQueue*> m_frameQueues;
    QTimer* m_inputDebounceTimer = nullptr;
    QTimer* m_progressUpdateTimer = nullptr;
    QTimer* m_detectionUpdateTimer = nullptr;
    QTimer* m_pipelineStatsTimer = nullptr;    // 性能监控定时器
    
    // 详情模式独立解码器（用于视频文件倍速控制，不影响网格模式检测）
    VideoDecoder* m_analysisDecoder = nullptr;
    QThread* m_analysisThread = nullptr;
    FrameQueue* m_analysisFrameQueue = nullptr;
    DisplayWorker* m_analysisWorker = nullptr;
    QThread* m_analysisDisplayThread = nullptr;

    // 推理检测
    std::vector<std::unique_ptr<Worker>> m_workers;
    bool m_detectionEnabled = false;
    bool m_modelLoaded = false;
    bool m_memoryInited = false;
    int m_workerCount = 0;
    int m_workerCountSetting = 0; // 0 = 自动模式，>0 = 手动指定
    int m_inferenceStreams = 2;
    int m_workerMaxBatch = 16;
    int m_contextPoolSize = 1; // 单 GPU 固定 1
    int m_baseSlots = 4; // 作为自动策略下限值
    QString m_modelPath = "/home/zzx/code/Qt/CudaForge-YOLO/src/engines/yolo26n.engine";
    std::vector<QString> m_classNames;
    QDateTime m_detectionStartTime;

    // 源切换历史（每个通道记录历次配置过的源路径）
    QMap<int, QStringList> m_sourceHistory;

    // 负载测试状态（真实解码模式）
    bool m_loadTestRealDecode = false;
    QVector<int> m_loadTestChannels;
    int m_loadTestTargetFps = 0;
    QMap<int, ChannelSettings> m_loadTestPrevSettings;
    QSet<int> m_loadTestPrevRunning;
    nvtxRangeId_t m_nvtxLoadTestRangeId = 0;

    // 高级设置
    QPushButton* m_btnAdvancedSettings = nullptr;
    AdvancedSettingsDialog* m_advancedDialog = nullptr;
    QString m_classesPath = "src/engines/class.txt";

    QWidget* m_customTitleBar = nullptr;
    QLabel* m_mainTitleLabel = nullptr;
    QPushButton* m_btnTitleMin = nullptr;
    QPushButton* m_btnTitleMax = nullptr;
    QPushButton* m_btnTitleClose = nullptr;
    bool m_titleDragging = false;
    QPoint m_titleDragOffset;
};

#endif // MAINWINDOW_H
