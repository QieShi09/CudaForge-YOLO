#ifndef DISPLAY_MANAGER_HPP
#define DISPLAY_MANAGER_HPP

#include <QObject>
#include <QImage>
#include <QMap>
#include <mutex>
#include <QThread>
#include <atomic>
#include "src/core/FrameQueue.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

// 独立的显示工作线程类
class DisplayWorker : public QObject {
    Q_OBJECT
public:
    DisplayWorker(int channel_id, FrameQueue* queue);
    ~DisplayWorker();

public Q_SLOTS:
    // 工作循环 (在独立线程中运行)
    void processLoop();

    // 控制是否执行渲染转换
    void setRenderingEnabled(bool enabled) { rendering_enabled_ = enabled; }

Q_SIGNALS:
    void finished(); // 线程结束信号

public:
    // 共享数据接口
    std::atomic<void*> current_ptr{nullptr};
    std::atomic<int> current_w{0};
    std::atomic<int> current_h{0};
    std::atomic<int> current_pitch{0};
    std::atomic<int> current_format{AV_PIX_FMT_NONE};

private:
    int channel_id_;
    FrameQueue* queue_;
    
    std::atomic<bool> rendering_enabled_{true}; // 渲染开关

    // 每个线程独立持有资源，无需锁
    struct SwsContextInfo {
        SwsContext* ctx = nullptr;
        int width = 0;
        int height = 0;
        int format = 0;
    } sws_;
    
    void* gpu_buffer_ = nullptr;
    int gpu_width_ = 0;
    int gpu_height_ = 0;
    size_t gpu_buffer_bytes_ = 0;

};

class DisplayManager : public QObject {
    Q_OBJECT
public:
    explicit DisplayManager(QObject *parent = nullptr);
    ~DisplayManager();

    // 注册/注销通道的帧队列
    void addChannel(int channel_id, FrameQueue* queue);
    void removeChannel(int channel_id);
    
    // 获取 Worker 指针以便绑定 VideoWidget
    DisplayWorker* getWorker(int channel_id);

Q_SIGNALS:

private:
    std::mutex mutex_;
    
    // 管理所有通道的线程和工人
    QMap<int, QThread*> threads_;
    QMap<int, DisplayWorker*> workers_;
};

#endif // DISPLAY_MANAGER_HPP