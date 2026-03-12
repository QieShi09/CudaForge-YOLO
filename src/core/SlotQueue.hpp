#ifndef SLOT_QUEUE_HPP
#define SLOT_QUEUE_HPP

#include <deque>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <cstddef>
#include <chrono>
#include <functional>
#include <unordered_set>
#include "Slot.hpp"
#include "MemoryManager.hpp"

class SlotQueue {
public:
    static SlotQueue& getInstance() {
        static SlotQueue inst;
        return inst;
    }

    using FillSlotFn = std::function<bool(Slot*, int)>;

    void init(size_t max_items, size_t batch_capacity = 1,
              std::chrono::microseconds partial_flush_age = std::chrono::microseconds(20000)) {
        std::lock_guard<std::mutex> lk(m_);
        max_items_ = max_items;
        batch_capacity_ = std::max<size_t>(1, batch_capacity);
        partial_flush_age_ = partial_flush_age;
        stop_ = false;
        disabled_channels_.clear();
        enabled_.store(true);
        if (filling_slot_) {
            MemoryManager::getInstance().release(filling_slot_);
            filling_slot_ = nullptr;
        }
    }
    void enable() { enabled_.store(true); }
    void disable() { enabled_.store(false); }
    bool isEnabled() const { return enabled_.load(); }

    void disableChannel(int channel_id) {
        std::lock_guard<std::mutex> lk(m_);
        disabled_channels_.insert(channel_id);
    }

    void enableChannel(int channel_id) {
        std::lock_guard<std::mutex> lk(m_);
        disabled_channels_.erase(channel_id);
    }

    bool isChannelEnabled(int channel_id) const {
        if (!isEnabled()) return false;
        std::lock_guard<std::mutex> lk(m_);
        return disabled_channels_.find(channel_id) == disabled_channels_.end();
    }

    struct Item {
        Slot* slot = nullptr;
        int channel_id = 0;
        uint64_t epoch = 0;
    };

    bool appendSample(FillSlotFn fill_fn, int channel_id, uint64_t epoch, const Slot::PreprocMeta& meta) {
        if (!fill_fn) return false;
        if (!isEnabled()) return false;

        std::lock_guard<std::mutex> lk(m_);
        if (disabled_channels_.find(channel_id) != disabled_channels_.end()) return false;

        auto now = std::chrono::steady_clock::now();

        if (!ensureFillingSlotLocked(now)) return false;

        if (!filling_slot_->canAppendSample()) {
            if (!enqueueReadySlotLocked(filling_slot_)) {
                filling_slot_ = nullptr;
                return false;
            }
            filling_slot_ = nullptr;
            if (!ensureFillingSlotLocked(now)) return false;
        }

        const int sample_index = filling_slot_->getNextSampleIndex();
        if (!fill_fn(filling_slot_, sample_index)) {
            if (filling_slot_ && filling_slot_->getCurBatchSize() == 0) {
                MemoryManager::getInstance().release(filling_slot_);
                filling_slot_ = nullptr;
            }
            return false;
        }

        if (!filling_slot_->appendSampleMeta(channel_id, epoch, meta)) {
            if (filling_slot_ && filling_slot_->getCurBatchSize() == 0) {
                MemoryManager::getInstance().release(filling_slot_);
                filling_slot_ = nullptr;
            }
            return false;
        }

        if (static_cast<size_t>(filling_slot_->getCurBatchSize()) >= std::min(batch_capacity_, static_cast<size_t>(filling_slot_->getBatchCapacity()))) {
            if (!enqueueReadySlotLocked(filling_slot_)) {
                filling_slot_ = nullptr;
                return false;
            }
            filling_slot_ = nullptr;
        }
        return true;
    }

    bool pushSlot(Slot* slot, int channel_id = 0, uint64_t epoch = 0) {
        if (!slot) return false;
        if (!isEnabled()) return false;
        std::lock_guard<std::mutex> lk(m_);
        if (disabled_channels_.find(channel_id) != disabled_channels_.end()) return false;
        return enqueueReadySlotLocked(slot, channel_id, epoch);
    }

    Item pop() {
        std::unique_lock<std::mutex> lk(m_);
        cv_.wait(lk, [this]{ return !queue_.empty() || stop_; });
        if (queue_.empty()) return Item();
        Item f = queue_.front();
        queue_.pop_front();
        return f;
    }

    Item pop_for(std::chrono::milliseconds ms) {
        std::unique_lock<std::mutex> lk(m_);
        if (!cv_.wait_for(lk, ms, [this]{ return !queue_.empty() || stop_; })) return Item();
        if (queue_.empty()) return Item();
        Item f = queue_.front();
        queue_.pop_front();
        return f;
    }

    std::vector<Item> pop_bulk(size_t max_n, std::chrono::milliseconds ms) {
        std::unique_lock<std::mutex> lk(m_);
        if (!cv_.wait_for(lk, ms, [this]{ return !queue_.empty() || stop_; })) return {};
        std::vector<Item> out;
        size_t n = std::min(max_n, queue_.size());
        out.reserve(n);
        for (size_t i = 0; i < n; ++i) {
            out.push_back(queue_.front());
            queue_.pop_front();
        }
        return out;
    }

    std::vector<Item> pop_bulk_nowait(size_t max_n) {
        std::lock_guard<std::mutex> lk(m_);
        if (queue_.empty()) return {};
        std::vector<Item> out;
        size_t n = std::min(max_n, queue_.size());
        out.reserve(n);
        for (size_t i = 0; i < n; ++i) {
            out.push_back(queue_.front());
            queue_.pop_front();
        }
        return out;
    }

    void stop() {
        std::lock_guard<std::mutex> lk(m_);
        stop_ = true;
        enabled_.store(false);
        cv_.notify_all();
    }

    void flushPending() {
        std::lock_guard<std::mutex> lk(m_);
        flushPendingLocked();
    }

    void flushPendingIfStale() {
        std::lock_guard<std::mutex> lk(m_);
        flushPendingIfStaleLocked(std::chrono::steady_clock::now());
    }

    void clear() {
        std::lock_guard<std::mutex> lk(m_);
        if (filling_slot_) {
            MemoryManager::getInstance().release(filling_slot_);
            filling_slot_ = nullptr;
        }
        while (!queue_.empty()) {
            Item it = queue_.front();
            queue_.pop_front();
            if (it.slot) MemoryManager::getInstance().release(it.slot);
        }
    }

    void clearChannel(int channel_id) {
        std::lock_guard<std::mutex> lk(m_);
        if (filling_slot_ && slotContainsOnlyChannel(filling_slot_, channel_id)) {
            MemoryManager::getInstance().release(filling_slot_);
            filling_slot_ = nullptr;
        }
        for (auto it = queue_.begin(); it != queue_.end(); ) {
            if (it->slot && slotContainsOnlyChannel(it->slot, channel_id)) {
                if (it->slot) MemoryManager::getInstance().release(it->slot);
                it = queue_.erase(it);
            } else {
                ++it;
            }
        }
    }

    size_t size() const {
        std::lock_guard<std::mutex> lk(m_);
        return queue_.size() + ((filling_slot_ && filling_slot_->getCurBatchSize() > 0) ? 1 : 0);
    }

    size_t capacity() const { return max_items_; }

private:
    SlotQueue() = default;
    ~SlotQueue() = default;
    SlotQueue(const SlotQueue&) = delete;
    SlotQueue& operator=(const SlotQueue&) = delete;

    enum class OverflowPolicy { DropNew = 0, OverwriteOld = 1 };
    OverflowPolicy overflow_policy_ = OverflowPolicy::DropNew;
    std::deque<Item> queue_;
    mutable std::mutex m_;
    std::condition_variable cv_;
    size_t max_items_ = 128;
    size_t batch_capacity_ = 1;
    std::atomic<bool> enabled_{false};
    std::unordered_set<int> disabled_channels_;
    Slot* filling_slot_ = nullptr;
    std::chrono::steady_clock::time_point filling_slot_started_{};
    std::chrono::microseconds partial_flush_age_{1500};
    bool stop_ = false;

    bool ensureFillingSlotLocked(std::chrono::steady_clock::time_point now) {
        if (filling_slot_) return true;
        filling_slot_ = MemoryManager::getInstance().tryAcquire();
        if (!filling_slot_) return false;
        filling_slot_->clear();
        filling_slot_started_ = now;
        return true;
    }

    bool enqueueReadySlotLocked(Slot* slot, int channel_id = 0, uint64_t epoch = 0) {
        if (!slot) return false;
        if (queue_.size() >= max_items_) {
            if (overflow_policy_ == OverflowPolicy::DropNew) {
                MemoryManager::getInstance().release(slot);
                return false;
            } else {
                Item old = queue_.front();
                queue_.pop_front();
                if (old.slot) MemoryManager::getInstance().release(old.slot);
            }
        }
        slot->markState(Slot::State::Ready);
        Item it;
        it.slot = slot;
        it.channel_id = channel_id;
        it.epoch = epoch;
        queue_.push_back(it);
        cv_.notify_one();
        return true;
    }

    void flushPendingLocked() {
        if (!filling_slot_) return;
        if (filling_slot_->getCurBatchSize() <= 0) {
            MemoryManager::getInstance().release(filling_slot_);
            filling_slot_ = nullptr;
            return;
        }
        enqueueReadySlotLocked(filling_slot_);
        filling_slot_ = nullptr;
    }

    void flushPendingIfStaleLocked(std::chrono::steady_clock::time_point now) {
        if (!filling_slot_ || filling_slot_->getCurBatchSize() <= 0) return;
        if (now - filling_slot_started_ >= partial_flush_age_) {
            enqueueReadySlotLocked(filling_slot_);
            filling_slot_ = nullptr;
        }
    }

    bool slotContainsOnlyChannel(Slot* slot, int channel_id) const {
        if (!slot) return false;
        int count = slot->getCurBatchSize();
        if (count <= 0) return false;
        for (int i = 0; i < count; ++i) {
            if (slot->getSampleChannelId(i) != channel_id) return false;
        }
        return true;
    }
};

#endif
