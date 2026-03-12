#ifndef WORKER_HPP
#define WORKER_HPP

#include <vector>
#include <thread>
#include <atomic>
#include <chrono>

#include <cuda_runtime.h>

class Worker {
public:
    // max_batch: 一次尝试获取的最大样本数
    // max_wait_ms: 若队列为空，等待的最长毫秒数
    // total_workers: 总 worker 数量，用于选择 stream 同步策略
        Worker(int id, size_t max_batch, std::chrono::milliseconds max_wait_ms,
            int total_workers = 1, int stream_count = 1);
    ~Worker();

    void start();
    void stop();

private:
    void run();

    int id_ = 0;
    size_t max_batch_ = 1;
    std::chrono::milliseconds max_wait_ms_{50};
    int total_workers_ = 1;  // 用于选择 stream 同步策略
    int stream_count_ = 1;
    int stream_rr_ = 0;
    std::thread thr_;
    std::atomic<bool> running_{false};
    std::vector<cudaStream_t> streams_;
};

#endif
