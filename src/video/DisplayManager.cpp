#include "DisplayManager.hpp"
#include "../core/PipelineStats.hpp"
#include <QDebug>
#include <chrono>
#include <cstdio>
#include <cuda_runtime.h>

DisplayManager::DisplayManager(QObject *parent) : QObject(parent) {}

DisplayManager::~DisplayManager() {
    std::lock_guard<std::mutex> lock(mutex_);
}

void DisplayManager::addChannel(int channel_id, FrameQueue* queue) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (threads_.contains(channel_id)) return;

    QThread* thread = new QThread;
    DisplayWorker* worker = new DisplayWorker(channel_id, queue);
    worker->moveToThread(thread);

    // 线程生命周期管理
    connect(thread, &QThread::started, worker, &DisplayWorker::processLoop);
    connect(worker, &DisplayWorker::finished, thread, &QThread::quit);
    connect(thread, &QThread::finished, worker, &DisplayWorker::deleteLater);
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);

    threads_[channel_id] = thread;
    workers_[channel_id] = worker;

    thread->start();
}

void DisplayManager::removeChannel(int channel_id) {
    QThread* thread = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!threads_.contains(channel_id)) return;
        
        // 使用 take 将线程移出 Map，然后释放锁
        thread = threads_.take(channel_id);
        workers_.remove(channel_id);
    }

    if (thread) {
        // 在锁外等待，这样 UI 线程不会阻塞其他通道的操作
        thread->quit();
        thread->wait();
    }
}

DisplayWorker* DisplayManager::getWorker(int channel_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    return workers_.value(channel_id, nullptr);
}

DisplayWorker::DisplayWorker(int channel_id, FrameQueue* queue) 
    : channel_id_(channel_id), queue_(queue) {}

DisplayWorker::~DisplayWorker() {
    if (sws_.ctx) sws_freeContext(sws_.ctx);
    if (gpu_buffer_) cudaFree(gpu_buffer_);
}

void DisplayWorker::processLoop() {
    fprintf(stderr, "[Display ch=%d] processLoop started\n", channel_id_);
    fflush(stderr);
    // 声明 FPS 计数器变量
    int frame_count = 0;
    auto last_fps_time = std::chrono::steady_clock::now();

    while (true) {
        // 阻塞式取帧：如果队列空，线程会挂起，不占 CPU
        AVFrame* src_frame = queue_->pop();
        if (!src_frame) {
            fprintf(stderr, "[Display ch=%d] processLoop exiting (null frame)\n", channel_id_);
            fflush(stderr);
            break; // 队列停止，退出循环
        }
        int64_t pts = src_frame->pts;
        
        auto frame_start_time = std::chrono::steady_clock::now();

        int w = src_frame->width;
        int h = src_frame->height;

        // 如果关闭渲染，则跳过 CUDA 转换和 UI 更新
        if (!rendering_enabled_) {
            av_frame_free(&src_frame);
            // 依然保持简单的 FPS 统计，方便观察后台运行状态
            frame_count++;
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(now - last_fps_time).count() >= 1) {
                frame_count = 0;
                last_fps_time = now;
            }
            continue;
        }

        if (src_frame->format != AV_PIX_FMT_CUDA) {
            fprintf(stderr, "[Display ch=%d] Unexpected non-CUDA frame (fmt=%d), dropping.\n",
                    channel_id_, src_frame->format);
            av_frame_free(&src_frame);
            continue;
        }

        // 检查帧数据指针有效性
        if (!src_frame->data[0]) {
            fprintf(stderr, "[Display ch=%d] FATAL: src_frame->data[0] is NULL!\n", channel_id_);
            av_frame_free(&src_frame);
            continue;
        }

        bool has_uv_plane = (src_frame->data[1] != nullptr);
        if (has_uv_plane) {
            // NV12: 保持原格式，供 UI 直接渲染
            size_t nv12_size = static_cast<size_t>(w) * h * 3 / 2;
            if (gpu_width_ != w || gpu_height_ != h || gpu_buffer_bytes_ < nv12_size) {
                if (gpu_buffer_) { cudaFree(gpu_buffer_); gpu_buffer_ = nullptr; }
                cudaError_t err = cudaMalloc(&gpu_buffer_, nv12_size);
                if (err != cudaSuccess) {
                    fprintf(stderr, "[Display ch=%d] FATAL: cudaMalloc NV12 display buffer (%zu bytes) failed: %s\n",
                            channel_id_, nv12_size, cudaGetErrorString(err));
                    gpu_buffer_ = nullptr;
                    gpu_buffer_bytes_ = 0;
                    av_frame_free(&src_frame);
                    continue;
                }
                gpu_buffer_bytes_ = nv12_size;
                gpu_width_ = w;
                gpu_height_ = h;
            }
            uint8_t* d_y = reinterpret_cast<uint8_t*>(gpu_buffer_);
            uint8_t* d_uv = d_y + static_cast<size_t>(w) * h;
            cudaError_t e1 = cudaMemcpy2D(d_y, w, src_frame->data[0], src_frame->linesize[0], w, h, cudaMemcpyDeviceToDevice);
            cudaError_t e2 = cudaMemcpy2D(d_uv, w, src_frame->data[1], src_frame->linesize[1], w, h / 2, cudaMemcpyDeviceToDevice);
            if (e1 != cudaSuccess || e2 != cudaSuccess) {
                fprintf(stderr, "[Display ch=%d] cudaMemcpy2D NV12 failed: Y=%s UV=%s (data[0]=%p ls[0]=%d data[1]=%p ls[1]=%d %dx%d)\n",
                        channel_id_, cudaGetErrorString(e1), cudaGetErrorString(e2),
                        (void*)src_frame->data[0], src_frame->linesize[0],
                        (void*)src_frame->data[1], src_frame->linesize[1], w, h);
                av_frame_free(&src_frame);
                continue;
            }
            current_pitch.store(w, std::memory_order_relaxed);
            current_format.store(AV_PIX_FMT_NV12, std::memory_order_relaxed);
        } else {
            // Already RGBA on device: just copy into the persistent buffer for rendering
            size_t rgba_size = static_cast<size_t>(w) * h * 4;
            if (gpu_width_ != w || gpu_height_ != h || gpu_buffer_bytes_ < rgba_size) {
                if (gpu_buffer_) { cudaFree(gpu_buffer_); gpu_buffer_ = nullptr; }
                cudaError_t err = cudaMalloc(&gpu_buffer_, rgba_size);
                if (err != cudaSuccess) {
                    fprintf(stderr, "[Display ch=%d] FATAL: cudaMalloc RGBA display buffer (%zu bytes) failed: %s\n",
                            channel_id_, rgba_size, cudaGetErrorString(err));
                    gpu_buffer_ = nullptr;
                    gpu_buffer_bytes_ = 0;
                    av_frame_free(&src_frame);
                    continue;
                }
                gpu_buffer_bytes_ = rgba_size;
                gpu_width_ = w;
                gpu_height_ = h;
            }
            cudaError_t e = cudaMemcpy2D((uint8_t*)gpu_buffer_, w * 4,
                         src_frame->data[0], src_frame->linesize[0],
                         w * 4, h, cudaMemcpyDeviceToDevice);
            if (e != cudaSuccess) {
                fprintf(stderr, "[Display ch=%d] cudaMemcpy2D RGBA failed: %s (data[0]=%p ls[0]=%d %dx%d)\n",
                        channel_id_, cudaGetErrorString(e),
                        (void*)src_frame->data[0], src_frame->linesize[0], w, h);
                av_frame_free(&src_frame);
                continue;
            }
            current_pitch.store(w * 4, std::memory_order_relaxed);
            current_format.store(AV_PIX_FMT_RGBA, std::memory_order_relaxed);
        }

        // 更新原子变量，不发信号
        current_w.store(w, std::memory_order_relaxed);
        current_h.store(h, std::memory_order_relaxed);
        current_ptr.store(gpu_buffer_, std::memory_order_release); // 发布最新帧
        PipelineStats::getInstance().frames_displayed.fetch_add(1, std::memory_order_relaxed);

        av_frame_free(&src_frame);

        frame_count++;
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_fps_time).count() >= 1) {
            frame_count = 0;
            last_fps_time = now;
        }

    }
    
    Q_EMIT finished();
}
