#include "TensorArenaManager.hpp"

bool TensorArenaManager::init(size_t input_arena_bytes, size_t output_arena_bytes) {
    shutdown();
    if (input_arena_bytes == 0 || output_arena_bytes == 0) return false;

    std::lock_guard<std::mutex> lk(mutex_);
    if (!input_arena_.init(input_arena_bytes)) return false;
    if (!output_arena_.init(output_arena_bytes)) {
        input_arena_.shutdown();
        return false;
    }
    initialized_ = true;
    return true;
}

void TensorArenaManager::shutdown() {
    std::lock_guard<std::mutex> lk(mutex_);
    output_arena_.shutdown();
    input_arena_.shutdown();
    initialized_ = false;
}

void* TensorArenaManager::allocateInput(size_t bytes, size_t alignment) {
    std::lock_guard<std::mutex> lk(mutex_);
    if (!initialized_) return nullptr;
    return input_arena_.allocate(bytes, alignment);
}

void TensorArenaManager::deallocateInput(void* ptr, size_t bytes) {
    std::lock_guard<std::mutex> lk(mutex_);
    if (!initialized_) return;
    input_arena_.deallocate(ptr, bytes);
}

void TensorArenaManager::deallocateInputAfter(void* ptr, size_t bytes, cudaEvent_t event) {
    std::lock_guard<std::mutex> lk(mutex_);
    if (!initialized_) return;
    input_arena_.deallocate_after(ptr, bytes, event);
}

void* TensorArenaManager::allocateOutput(size_t bytes, size_t alignment) {
    std::lock_guard<std::mutex> lk(mutex_);
    if (!initialized_) return nullptr;
    return output_arena_.allocate(bytes, alignment);
}

void TensorArenaManager::deallocateOutput(void* ptr, size_t bytes) {
    std::lock_guard<std::mutex> lk(mutex_);
    if (!initialized_) return;
    output_arena_.deallocate(ptr, bytes);
}

void TensorArenaManager::deallocateOutputAfter(void* ptr, size_t bytes, cudaEvent_t event) {
    std::lock_guard<std::mutex> lk(mutex_);
    if (!initialized_) return;
    output_arena_.deallocate_after(ptr, bytes, event);
}

GpuArena::Stats TensorArenaManager::inputStats() const {
    std::lock_guard<std::mutex> lk(mutex_);
    return input_arena_.getStats();
}

GpuArena::Stats TensorArenaManager::outputStats() const {
    std::lock_guard<std::mutex> lk(mutex_);
    return output_arena_.getStats();
}
