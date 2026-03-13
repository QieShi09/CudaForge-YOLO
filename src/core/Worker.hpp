#ifndef WORKER_HPP
#define WORKER_HPP

#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <memory>
#include <vector>

#include <cuda_runtime.h>

#include "InputFrameArenaStore.hpp"

class TRTWorker;

class Worker {
public:
    // max_batch: 一次尝试获取的最大样本数
    // max_wait_ms: 若队列为空，等待的最长毫秒数
    Worker(int id, size_t max_batch, std::chrono::milliseconds max_wait_ms);
    ~Worker();

    void start();
    void stop();

private:
    void run();

    int id_ = 0;
    size_t max_batch_ = 1;
    std::chrono::milliseconds max_wait_ms_{50};
    std::thread thr_;
    std::atomic<bool> running_{false};
    std::atomic<int> inflight_infers_{0};
    int max_inflight_per_worker_ = 2;
    std::unique_ptr<TRTWorker> trt_worker_;
};

#endif
