#ifndef TENSOR_ARENA_MANAGER_HPP
#define TENSOR_ARENA_MANAGER_HPP

#include "GpuArena.hpp"
#include <cstddef>
#include <mutex>

class TensorArenaManager {
public:
    static TensorArenaManager& getInstance() {
        static TensorArenaManager inst;
        return inst;
    }

    bool init(size_t input_arena_bytes, size_t output_arena_bytes);
    void shutdown();

    void* allocateInput(size_t bytes, size_t alignment = 256);
    void deallocateInput(void* ptr, size_t bytes);
    void deallocateInputAfter(void* ptr, size_t bytes, cudaEvent_t event);

    void* allocateOutput(size_t bytes, size_t alignment = 256);
    void deallocateOutput(void* ptr, size_t bytes);
    void deallocateOutputAfter(void* ptr, size_t bytes, cudaEvent_t event);

    GpuArena::Stats inputStats() const;
    GpuArena::Stats outputStats() const;
    std::vector<GpuArena::Segment> inputSegments() const;
    std::vector<GpuArena::Segment> outputSegments() const;
    uintptr_t inputBaseAddress() const;
    uintptr_t outputBaseAddress() const;

private:
    TensorArenaManager() = default;
    ~TensorArenaManager() = default;
    TensorArenaManager(const TensorArenaManager&) = delete;
    TensorArenaManager& operator=(const TensorArenaManager&) = delete;

    mutable std::mutex mutex_;
    GpuArena input_arena_{"input-tensor-arena"};
    GpuArena output_arena_{"output-tensor-arena"};
    bool initialized_ = false;
};

#endif
