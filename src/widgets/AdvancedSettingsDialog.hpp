#ifndef ADVANCED_SETTINGS_DIALOG_HPP
#define ADVANCED_SETTINGS_DIALOG_HPP

#include <QDialog>
#include <QSettings>
#include <QTimer>
#include <atomic>

class QSpinBox;
class QLineEdit;
class QPushButton;
class QLabel;
class QProgressBar;
class QGroupBox;
class QComboBox;
namespace Ui { class AdvancedSettingsDialog; }

/**
 * @brief 高级设置弹窗
 * 功能：性能仪表盘实时显示、参数调节、设置持久化、恢复默认
 */
class AdvancedSettingsDialog : public QDialog
{
    Q_OBJECT
public:
    struct Settings {
        int baseSlots = 2;
        int workerCount = 0;        // 0 表示自动
        int workerMaxBatch = 16;
        int contextPoolSize = 0;    // 0 = 自动（VRAM-aware），>0 = 固定数量
        QString modelPath = "/home/zzx/code/Qt/CudaForge-YOLO/src/engines/yolo26n.engine";
        QString classesPath = "src/engines/class.txt";
        int statsInterval = 5;  // 性能统计间隔（秒）
    };

    explicit AdvancedSettingsDialog(QWidget *parent = nullptr);
    ~AdvancedSettingsDialog() override;

    // 获取当前设置
    Settings getSettings() const;

    // 从外部设置值（加载时用）
    void setSettings(const Settings& s);

    // 静态方法：持久化读写
    static Settings loadFromDisk();
    static void saveToDisk(const Settings& s);
    static Settings defaultSettings();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

Q_SIGNALS:
    void settingsApplied(const Settings& s);
    void loadTestStartRequested(bool useRealDecode, int numChannels, int targetFps, int layoutMode);   // 通知主窗口：启动压测
    void loadTestStopRequested();    // 通知主窗口：恢复通道

private Q_SLOTS:
    void onApply();
    void onRestoreDefaults();
    void refreshDashboard();
    void runLoadTest();
    void stopLoadTest();
    void copyDashboard();

private:
    void buildUI();
    void applyModernStyle();
    Ui::AdvancedSettingsDialog* m_ui = nullptr;

    QWidget* m_titleBar = nullptr;
    QLabel*  m_titleLabel = nullptr;
    QPushButton* m_btnCloseDialog = nullptr;
    bool m_dragging = false;
    QPoint m_dragOffset;

    // --- 参数输入控件 ---
    QSpinBox*    m_spinSlots = nullptr;
    QSpinBox*    m_spinWorkerCount = nullptr;
    QSpinBox*    m_spinBatch = nullptr;
    QSpinBox*    m_spinContextPool = nullptr;
    QLineEdit*   m_editModelPath = nullptr;
    QLineEdit*   m_editClassesPath = nullptr;
    QSpinBox*    m_spinStatsInterval = nullptr;
    QPushButton* m_btnBrowseModel = nullptr;
    QPushButton* m_btnBrowseClasses = nullptr;
    QPushButton* m_btnApply = nullptr;
    QPushButton* m_btnDefaults = nullptr;

    // --- 性能仪表盘控件 ---
    QLabel*       m_lblGpuMem = nullptr;
    QProgressBar* m_barGpuMem = nullptr;
    QLabel*       m_lblVramSlot = nullptr;
    QLabel*       m_lblVramOther = nullptr;
    QLabel*       m_lblVramCtx = nullptr;
    QLabel*       m_lblVramDecoder = nullptr;
    QLabel*       m_lblDecoderCount = nullptr;
    QLabel*       m_lblSlotPool = nullptr;
    QProgressBar* m_barSlotPool = nullptr;
    QLabel*       m_lblDetQueue = nullptr;
    QProgressBar* m_barDetQueue = nullptr;
    QLabel*       m_lblDecodeFps = nullptr;
    QLabel*       m_lblInferFps = nullptr;
    QLabel*       m_lblDisplayFps = nullptr;
    QLabel*       m_lblDetections = nullptr;
    QLabel*       m_lblDqDrop = nullptr;
    QLabel*       m_lblDqPush = nullptr;
    QLabel*       m_lblBatchUtil = nullptr;
    QLabel*       m_lblWorkerIdle = nullptr;
    QLabel*       m_lblSlotWait = nullptr;
    QLabel*       m_lblPreprocTime = nullptr;
    QLabel*       m_lblCtxPool = nullptr;
    QLabel*       m_lblPeakSlots = nullptr;
    QLabel*       m_lblBottleneck = nullptr;
    QLabel*       m_lblLoadTest = nullptr;
    QPushButton*  m_btnLoadTest = nullptr;
    QPushButton*  m_btnStopTest = nullptr;
    QPushButton*  m_btnCopyDash = nullptr;
    QComboBox*    m_comboLoadLayout = nullptr;
    QLabel*       m_lblLoadLayout = nullptr;
    QSpinBox*     m_spinLoadFps = nullptr;
    QSpinBox*     m_spinLoadDuration = nullptr;

    QTimer*       m_dashTimer = nullptr;
    QString       m_lastDashboardText;
    QStringList   m_dashboardHistory;   // 最近 N 次仪表盘快照

    // 压力测试运行时状态
    std::atomic<bool> m_loadTestRunning{false};
};

#endif // ADVANCED_SETTINGS_DIALOG_HPP
