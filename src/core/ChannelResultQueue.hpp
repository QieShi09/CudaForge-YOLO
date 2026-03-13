#ifndef CHANNEL_RESULT_QUEUE_HPP
#define CHANNEL_RESULT_QUEUE_HPP

#include "DetectionResults.hpp"
#include <unordered_map>
#include <deque>
#include <vector>
#include <mutex>
#include <cstdint>

class ChannelResultQueue {
public:
    struct Item {
        uint64_t epoch = 0;
        std::vector<DetectionResults::DetectionBox> detections;
    };

    static ChannelResultQueue& getInstance() {
        static ChannelResultQueue inst;
        return inst;
    }

    void push(int channel_id, Item item) {
        std::lock_guard<std::mutex> lk(mutex_);
        queues_[channel_id].push_back(std::move(item));
    }

    bool pop(int channel_id, Item& out) {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = queues_.find(channel_id);
        if (it == queues_.end() || it->second.empty()) return false;
        out = std::move(it->second.front());
        it->second.pop_front();
        return true;
    }

    bool popLatest(int channel_id, Item& out) {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = queues_.find(channel_id);
        if (it == queues_.end() || it->second.empty()) return false;
        out = std::move(it->second.back());
        it->second.clear();
        return true;
    }

    void clearChannel(int channel_id) {
        std::lock_guard<std::mutex> lk(mutex_);
        queues_.erase(channel_id);
    }

    void clearAll() {
        std::lock_guard<std::mutex> lk(mutex_);
        queues_.clear();
    }

private:
    ChannelResultQueue() = default;
    ~ChannelResultQueue() = default;
    ChannelResultQueue(const ChannelResultQueue&) = delete;
    ChannelResultQueue& operator=(const ChannelResultQueue&) = delete;

    std::unordered_map<int, std::deque<Item>> queues_;
    std::mutex mutex_;
};

#endif
