#ifndef GPU_ARENA_HPP
#define GPU_ARENA_HPP

#include <cuda_runtime_api.h>
#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>
#include <thread>
#include <vector>
#include <atomic>
#include <string>

class GpuArena {
public:
    enum class SegmentStatus : uint8_t {
        Free = 0,
        Active = 1,
        PendingFree = 2,
    };

    struct Segment {
        size_t offset = 0;
        size_t bytes = 0;
        SegmentStatus status = SegmentStatus::Free;
    };

    struct Stats {
        size_t total_bytes = 0;
        size_t used_bytes = 0;
        size_t free_bytes = 0;
        size_t largest_free_block = 0;
        size_t free_block_count = 0;
        size_t pending_free_count = 0;
        size_t pending_free_bytes = 0;
        size_t active_alloc_count = 0;
        size_t active_alloc_bytes = 0;
        double utilization = 0.0;
        double fragmentation_ratio = 0.0;
    };

    explicit GpuArena(const std::string& name = "arena");
    ~GpuArena();

    bool init(size_t total_bytes);
    void shutdown();

    void* allocate(size_t bytes, size_t alignment = 256);
    void deallocate(void* ptr, size_t bytes);
    void deallocate_after(void* ptr, size_t bytes, cudaEvent_t event);

    Stats getStats() const;
    double getFragmentationRatio() const;
    std::vector<Segment> getSegments() const;
    uintptr_t baseAddress() const;
    bool isInitialized() const { return initialized_.load(std::memory_order_relaxed); }

private:
    struct PendingFree {
        void* ptr = nullptr;
        size_t bytes = 0;
        cudaEvent_t event = nullptr;
    };

    static size_t alignUp(size_t value, size_t alignment);
    void mergeAround(std::map<size_t, size_t>::iterator it);
    void recyclerLoop();

    std::string name_;
    void* base_ptr_ = nullptr;
    size_t total_bytes_ = 0;

    mutable std::mutex mutex_;
    std::map<size_t, size_t> free_blocks_;
    std::map<size_t, size_t> active_blocks_;
    size_t used_bytes_ = 0;

    std::vector<PendingFree> pending_frees_;
    std::thread recycler_thread_;
    std::atomic<bool> recycler_running_{false};
    std::atomic<bool> initialized_{false};
};

#endif
