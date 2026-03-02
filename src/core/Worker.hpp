#ifndef WORKER_HPP
#define WORKER_HPP

#include <vector>
#include <thread>
#include <atomic>
#include <chrono>

#include <cuda_runtime.h>

extern "C" {
#include <libavutil/frame.h>
}

class Worker {
public:
    // max_batch: 一次尝试获取的最大样本数
    // max_wait_ms: 若队列为空，等待的最长毫秒数
    // total_workers: 总 worker 数量，用于选择 stream 同步策略
    Worker(int id, size_t max_batch, std::chrono::milliseconds max_wait_ms, int total_workers = 1);
    ~Worker();

    void start();
    void stop();

private:
    void run();

    int id_ = 0;
    size_t max_batch_ = 1;
    std::chrono::milliseconds max_wait_ms_{50};
    int total_workers_ = 1;  // 用于选择 stream 同步策略
    std::thread thr_;
    std::atomic<bool> running_{false};
    // 每个 worker 在 start 时创建并持有一个 cudaStream，用于复用多次推理提交
    cudaStream_t stream_ = nullptr;
    // per-worker reusable NV12 临时缓冲，避免每帧分配
    uint8_t* tmp_dev_y_ = nullptr;
    uint8_t* tmp_dev_uv_ = nullptr;
    size_t tmp_dev_y_bytes_ = 0;
    size_t tmp_dev_uv_bytes_ = 0;
};

#endif
