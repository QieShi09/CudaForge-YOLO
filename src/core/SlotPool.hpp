#ifndef SLOT_POOL_HPP
#define SLOT_POOL_HPP

#include "Slot.hpp"
#include <deque>
#include <vector>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <atomic>

class SlotPool {
public:
    static SlotPool& getInstance() {
        static SlotPool inst;
        return inst;
    }

    bool init(size_t slot_count, int batch_capacity = 1);
    void shutdown();

    Slot* pop();
    Slot* tryPop();
    void push(Slot* slot);

    size_t totalSlots() const;
    size_t freeSlots() const;
    size_t activeSlots() const;

private:
    SlotPool() = default;
    ~SlotPool();
    SlotPool(const SlotPool&) = delete;
    SlotPool& operator=(const SlotPool&) = delete;

    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<std::unique_ptr<Slot>> all_slots_;
    std::deque<Slot*> free_slots_;
    std::atomic<bool> stop_{false};
};

#endif
