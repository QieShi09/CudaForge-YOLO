#include "SlotPool.hpp"

SlotPool::~SlotPool() {
    shutdown();
}

bool SlotPool::init(size_t slot_count, int batch_capacity) {
    shutdown();
    if (slot_count == 0) return false;

    std::lock_guard<std::mutex> lk(mutex_);
    stop_.store(false, std::memory_order_release);
    peak_active_slots_.store(0, std::memory_order_relaxed);
    all_slots_.reserve(slot_count);
    free_slots_.clear();

    for (size_t i = 0; i < slot_count; ++i) {
        std::unique_ptr<Slot> slot(new Slot());
        slot->id_ = static_cast<int>(i);
        slot->batch_capacity_ = (batch_capacity > 0) ? batch_capacity : 1;
        slot->cur_batch_size_ = 0;
        slot->state_.store(Slot::State::Free, std::memory_order_relaxed);
        if (!slot->event_) {
            cudaEventCreateWithFlags(&slot->event_, cudaEventDisableTiming);
        }
        free_slots_.push_back(slot.get());
        all_slots_.push_back(std::move(slot));
    }

    return true;
}

void SlotPool::shutdown() {
    std::lock_guard<std::mutex> lk(mutex_);
    stop_.store(true, std::memory_order_release);
    cv_.notify_all();

    for (auto& slot : all_slots_) {
        if (slot && slot->event_) {
            cudaEventDestroy(slot->event_);
            slot->event_ = nullptr;
        }
    }

    free_slots_.clear();
    all_slots_.clear();
    peak_active_slots_.store(0, std::memory_order_relaxed);
}

Slot* SlotPool::pop() {
    std::unique_lock<std::mutex> lk(mutex_);
    cv_.wait(lk, [this] {
        return stop_.load(std::memory_order_acquire) || !free_slots_.empty();
    });
    if (stop_.load(std::memory_order_acquire) || free_slots_.empty()) return nullptr;
    Slot* slot = free_slots_.front();
    free_slots_.pop_front();
    size_t active = (all_slots_.size() >= free_slots_.size()) ? (all_slots_.size() - free_slots_.size()) : 0;
    size_t peak = peak_active_slots_.load(std::memory_order_relaxed);
    while (active > peak && !peak_active_slots_.compare_exchange_weak(peak, active, std::memory_order_relaxed)) {}
    if (slot) slot->clear();
    return slot;
}

Slot* SlotPool::tryPop() {
    std::lock_guard<std::mutex> lk(mutex_);
    if (stop_.load(std::memory_order_acquire) || free_slots_.empty()) return nullptr;
    Slot* slot = free_slots_.front();
    free_slots_.pop_front();
    size_t active = (all_slots_.size() >= free_slots_.size()) ? (all_slots_.size() - free_slots_.size()) : 0;
    size_t peak = peak_active_slots_.load(std::memory_order_relaxed);
    while (active > peak && !peak_active_slots_.compare_exchange_weak(peak, active, std::memory_order_relaxed)) {}
    if (slot) slot->clear();
    return slot;
}

void SlotPool::push(Slot* slot) {
    if (!slot) return;
    std::lock_guard<std::mutex> lk(mutex_);
    slot->clear();
    free_slots_.push_back(slot);
    cv_.notify_one();
}

size_t SlotPool::totalSlots() const {
    std::lock_guard<std::mutex> lk(mutex_);
    return all_slots_.size();
}

size_t SlotPool::freeSlots() const {
    std::lock_guard<std::mutex> lk(mutex_);
    return free_slots_.size();
}

size_t SlotPool::activeSlots() const {
    std::lock_guard<std::mutex> lk(mutex_);
    return (all_slots_.size() >= free_slots_.size()) ? (all_slots_.size() - free_slots_.size()) : 0;
}

size_t SlotPool::peakActiveSlots() const {
    return peak_active_slots_.load(std::memory_order_relaxed);
}

size_t SlotPool::peakActiveSlotsAndReset() {
    return peak_active_slots_.exchange(0, std::memory_order_relaxed);
}
