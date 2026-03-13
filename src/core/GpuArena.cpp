#include "GpuArena.hpp"

#include <algorithm>
#include <chrono>

GpuArena::GpuArena(const std::string& name)
    : name_(name) {}

GpuArena::~GpuArena() {
    shutdown();
}

size_t GpuArena::alignUp(size_t value, size_t alignment) {
    if (alignment <= 1) return value;
    return (value + alignment - 1) / alignment * alignment;
}

bool GpuArena::init(size_t total_bytes) {
    shutdown();
    if (total_bytes == 0) return false;

    void* ptr = nullptr;
    cudaError_t err = cudaMalloc(&ptr, total_bytes);
    if (err != cudaSuccess || !ptr) {
        return false;
    }

    {
        std::lock_guard<std::mutex> lk(mutex_);
        base_ptr_ = ptr;
        total_bytes_ = total_bytes;
        used_bytes_ = 0;
        free_blocks_.clear();
        free_blocks_.emplace(0, total_bytes_);
        pending_frees_.clear();
    }

    recycler_running_.store(true, std::memory_order_release);
    recycler_thread_ = std::thread(&GpuArena::recyclerLoop, this);
    initialized_.store(true, std::memory_order_release);
    return true;
}

void GpuArena::shutdown() {
    recycler_running_.store(false, std::memory_order_release);
    if (recycler_thread_.joinable()) recycler_thread_.join();

    std::lock_guard<std::mutex> lk(mutex_);
    pending_frees_.clear();
    free_blocks_.clear();
    used_bytes_ = 0;
    if (base_ptr_) {
        cudaFree(base_ptr_);
        base_ptr_ = nullptr;
    }
    total_bytes_ = 0;
    initialized_.store(false, std::memory_order_release);
}

void* GpuArena::allocate(size_t bytes, size_t alignment) {
    if (!initialized_.load(std::memory_order_acquire)) return nullptr;
    size_t need = alignUp(bytes, alignment);
    if (need == 0) return nullptr;

    std::lock_guard<std::mutex> lk(mutex_);
    if (!base_ptr_) return nullptr;

    auto worst_it = free_blocks_.end();
    size_t worst_size = 0;

    for (auto it = free_blocks_.begin(); it != free_blocks_.end(); ++it) {
        size_t block_offset = it->first;
        size_t block_size = it->second;
        size_t aligned_offset = alignUp(block_offset, alignment);
        if (aligned_offset < block_offset) continue;
        size_t pad = aligned_offset - block_offset;
        if (block_size < pad || (block_size - pad) < need) continue;
        if (block_size > worst_size) {
            worst_size = block_size;
            worst_it = it;
        }
    }

    if (worst_it == free_blocks_.end()) return nullptr;

    size_t block_offset = worst_it->first;
    size_t block_size = worst_it->second;
    free_blocks_.erase(worst_it);

    size_t aligned_offset = alignUp(block_offset, alignment);
    size_t pad = aligned_offset - block_offset;

    if (pad > 0) {
        free_blocks_.emplace(block_offset, pad);
    }
    size_t tail_offset = aligned_offset + need;
    size_t consumed = pad + need;
    if (block_size > consumed) {
        free_blocks_.emplace(tail_offset, block_size - consumed);
    }

    used_bytes_ += need;
    return static_cast<void*>(static_cast<uint8_t*>(base_ptr_) + aligned_offset);
}

void GpuArena::mergeAround(std::map<size_t, size_t>::iterator it) {
    if (it == free_blocks_.end()) return;

    while (it != free_blocks_.begin()) {
        auto prev = std::prev(it);
        if (prev->first + prev->second != it->first) break;
        size_t new_offset = prev->first;
        size_t new_size = prev->second + it->second;
        free_blocks_.erase(it);
        free_blocks_.erase(prev);
        it = free_blocks_.emplace(new_offset, new_size).first;
    }

    auto next = std::next(it);
    while (next != free_blocks_.end() && (it->first + it->second == next->first)) {
        size_t new_offset = it->first;
        size_t new_size = it->second + next->second;
        free_blocks_.erase(next);
        free_blocks_.erase(it);
        it = free_blocks_.emplace(new_offset, new_size).first;
        next = std::next(it);
    }
}

void GpuArena::deallocate(void* ptr, size_t bytes) {
    if (!ptr || bytes == 0) return;
    if (!initialized_.load(std::memory_order_acquire)) return;

    std::lock_guard<std::mutex> lk(mutex_);
    if (!base_ptr_) return;

    uintptr_t base = reinterpret_cast<uintptr_t>(base_ptr_);
    uintptr_t p = reinterpret_cast<uintptr_t>(ptr);
    if (p < base) return;
    size_t offset = static_cast<size_t>(p - base);
    if (offset >= total_bytes_) return;

    size_t clamped = std::min(bytes, total_bytes_ - offset);
    auto it = free_blocks_.emplace(offset, clamped).first;
    mergeAround(it);

    if (used_bytes_ >= clamped) used_bytes_ -= clamped;
    else used_bytes_ = 0;
}

void GpuArena::deallocate_after(void* ptr, size_t bytes, cudaEvent_t event) {
    if (!ptr || bytes == 0) return;
    if (!event) {
        deallocate(ptr, bytes);
        return;
    }
    if (!initialized_.load(std::memory_order_acquire)) return;

    std::lock_guard<std::mutex> lk(mutex_);
    pending_frees_.push_back(PendingFree{ptr, bytes, event});
}

void GpuArena::recyclerLoop() {
    while (recycler_running_.load(std::memory_order_acquire)) {
        std::vector<PendingFree> ready;
        {
            std::lock_guard<std::mutex> lk(mutex_);
            if (!pending_frees_.empty()) {
                auto it = pending_frees_.begin();
                while (it != pending_frees_.end()) {
                    cudaError_t q = cudaEventQuery(it->event);
                    if (q == cudaSuccess) {
                        ready.push_back(*it);
                        it = pending_frees_.erase(it);
                    } else if (q != cudaErrorNotReady) {
                        ready.push_back(*it);
                        it = pending_frees_.erase(it);
                    } else {
                        ++it;
                    }
                }
            }
        }

        for (auto& task : ready) {
            deallocate(task.ptr, task.bytes);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

GpuArena::Stats GpuArena::getStats() const {
    Stats stats;
    std::lock_guard<std::mutex> lk(mutex_);
    stats.total_bytes = total_bytes_;
    stats.used_bytes = used_bytes_;
    stats.free_bytes = (total_bytes_ > used_bytes_) ? (total_bytes_ - used_bytes_) : 0;
    stats.free_block_count = free_blocks_.size();
    size_t largest = 0;
    for (const auto& kv : free_blocks_) {
        largest = std::max(largest, kv.second);
    }
    stats.largest_free_block = largest;
    if (stats.total_bytes > 0) {
        stats.utilization = static_cast<double>(stats.used_bytes) / static_cast<double>(stats.total_bytes);
    }
    if (stats.free_bytes > 0) {
        stats.fragmentation_ratio = static_cast<double>(stats.free_block_count) / static_cast<double>(stats.free_bytes);
    }
    return stats;
}

double GpuArena::getFragmentationRatio() const {
    return getStats().fragmentation_ratio;
}
