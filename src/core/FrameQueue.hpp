#ifndef FRAME_QUEUE_HPP
#define FRAME_QUEUE_HPP

#include <queue>
#include <mutex>
#include <condition_variable>

extern "C" {
#include <libavcodec/avcodec.h>
}

class FrameQueue {
public:
    // max_size: 缓冲池大小，建议 3-5 帧
    explicit FrameQueue(size_t max_size = 5);
    ~FrameQueue();

    // [生产者调用] 阻塞式推入
    // 如果队列满了，会阻塞直到有空间
    // 返回 false 表示队列已停止/销毁
    bool push(AVFrame* frame);

    // [生产者调用] 非阻塞推入（队列满时覆盖最旧帧）
    // 适用于实时流：宁可丢旧帧，也不要阻塞解码线程导致硬解 surface 枯竭
    // 返回 false 表示队列已停止/销毁
    bool pushDropOldest(AVFrame* frame);

    // [消费者调用] 阻塞式取出，队列停止时返回 nullptr
    AVFrame* pop();

    // 清空队列 (Seek 时调用)
    void clear();

    // 停止队列 (唤醒所有阻塞线程)
    void stop();
    void start();

    // 当前队列中的帧数（用于监控）
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

private:
    std::queue<AVFrame*> queue_;
    size_t max_size_;
    mutable std::mutex mutex_;
    std::condition_variable cond_not_full_;
    std::condition_variable cond_not_empty_;
    bool stop_flag_ = false;
};

#endif // FRAME_QUEUE_HPP