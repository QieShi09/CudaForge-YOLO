#include "FrameQueue.hpp"

FrameQueue::FrameQueue(size_t max_size) : max_size_(max_size) {}

FrameQueue::~FrameQueue() {
    stop();
    clear();
}

bool FrameQueue::push(AVFrame* frame) {
    std::unique_lock<std::mutex> lock(mutex_);
    // 等待队列不满
    cond_not_full_.wait(lock, [this] { return queue_.size() < max_size_ || stop_flag_; });

    if (stop_flag_) {
        return false;
    }

    queue_.push(frame);
    cond_not_empty_.notify_one(); // 通知消费者有货了
    return true;
}

bool FrameQueue::pushDropOldest(AVFrame* frame) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (stop_flag_) {
        return false;
    }

    if (queue_.size() >= max_size_) {
        AVFrame* old = queue_.front();
        queue_.pop();
        if (old) av_frame_free(&old);
    }

    queue_.push(frame);
    cond_not_empty_.notify_one();
    return true;
}

AVFrame* FrameQueue::pop() {
    std::unique_lock<std::mutex> lock(mutex_);
    
    // 使用条件变量阻塞等待，直到队列不为空或收到停止信号
    cond_not_empty_.wait(lock, [this] { return !queue_.empty() || stop_flag_; });

    if (stop_flag_ && queue_.empty()) {
        return nullptr; // 只有在停止且队列为空时才返回 nullptr
    }

    AVFrame* frame = queue_.front();
    queue_.pop();
    cond_not_full_.notify_one(); // 通知生产者有空位了
    return frame;
}

void FrameQueue::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    while (!queue_.empty()) {
        AVFrame* frame = queue_.front();
        queue_.pop();
        av_frame_free(&frame);
    }
    cond_not_full_.notify_all(); // 唤醒可能卡住的生产者
}

void FrameQueue::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    stop_flag_ = true;
    cond_not_full_.notify_all();
    cond_not_empty_.notify_all();
}

void FrameQueue::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    stop_flag_ = false;
}