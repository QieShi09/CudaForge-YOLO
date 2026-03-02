#include "PacketQueue.hpp"

void PacketQueue::push(AVPacket* pkt) {
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.push(pkt);
    cond_.notify_one();
}

AVPacket* PacketQueue::pop() {
    std::unique_lock<std::mutex> lock(mutex_);
    // 如果队列空了，就挂起线程等待，直到有新包进来或者收到停止信号
    cond_.wait(lock, [this] { return !queue_.empty() || stop_flag_; });
    
    if (stop_flag_ && queue_.empty()) return nullptr;

    AVPacket* pkt = queue_.front();
    queue_.pop();
    return pkt;
}

void PacketQueue::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    while (!queue_.empty()) {
        AVPacket* pkt = queue_.front();
        queue_.pop();
        av_packet_free(&pkt);
    }
}

void PacketQueue::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    stop_flag_ = true;
    cond_.notify_all();
}

void PacketQueue::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    stop_flag_ = false;
}

bool PacketQueue::isEmpty() {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.empty();
}