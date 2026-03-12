#include "MemoryManager.hpp"
#include <unordered_set>
#include <algorithm>
#include <cstdlib>

namespace {
int getSlotLimit() {
    int limit = 256;
    if (const char* env = std::getenv("CUDAFORGE_MAX_SLOTS")) {
        int v = std::atoi(env);
        if (v > 0) limit = std::clamp(v, 1, 1024);
    }
    return limit;
}
}

bool MemoryManager::init(int max_slots, size_t input_tensor_bytes, size_t output_tensor_bytes,
                         size_t nv12_bytes, int nv12_w, int nv12_h, int samples_per_slot) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (is_initialized_) {
        std::cerr << "[MemoryManager] Warning: Already initialized." << std::endl;
        return true;
    }

    try {
        int slot_limit = getSlotLimit();
        int req_slots = max_slots;
        max_slots = std::clamp(max_slots, 1, slot_limit);
        if (req_slots != max_slots) {
            std::cout << "[MemoryManager] Clamp init slots: " << req_slots
                      << " -> " << max_slots << " (limit=" << slot_limit << ")" << std::endl;
        }
        base_slots_ = max_slots;
        input_tensor_bytes_ = input_tensor_bytes;
        output_tensor_bytes_ = output_tensor_bytes;
        nv12_bytes_ = nv12_bytes;
        nv12_w_ = nv12_w;
        nv12_h_ = nv12_h;
        samples_per_slot_ = std::max(1, samples_per_slot);
        // 初始分配 base_slots_
        for (int i = 0; i < max_slots; ++i) {
            // 使用 friend class 特权创建 Slot
            Slot* slot = new Slot();
            slot->id_ = i;

            // 初始化新加入的元数据字段为默认值（确保后续使用之前状态可预测）
            slot->cur_batch_size_ = 0;
            slot->batch_capacity_ = samples_per_slot_;
            slot->metas_.clear();
            slot->sample_channel_ids_.clear();
            slot->sample_epochs_.clear();
            slot->stream_ = nullptr;
            slot->state_.store(Slot::State::Free);

            // 1. 分发推理输入空间 (device_b_ptr_)
            cudaError_t err_in = cudaMalloc(&slot->device_b_ptr_, input_tensor_bytes);

            // 2. 分发推理输出空间 (device_c_ptr_)
            cudaError_t err_out = cudaMalloc(&slot->device_c_ptr_, output_tensor_bytes);

            cudaError_t err_nv12 = cudaSuccess;
            if (nv12_bytes_ > 0) {
                err_nv12 = cudaMalloc(&slot->device_nv12_ptr_, nv12_bytes_ * static_cast<size_t>(samples_per_slot_));
                slot->nv12_w_ = nv12_w_;
                slot->nv12_h_ = nv12_h_;
                slot->nv12_pitch_ = nv12_w_;
            }

            // 3. 创建进度跟踪 Event (使用 DisableTiming 标志以提升性能)
            cudaError_t err_ev = cudaEventCreateWithFlags(&slot->event_, cudaEventDisableTiming);

            if (err_in != cudaSuccess || err_out != cudaSuccess || err_nv12 != cudaSuccess || err_ev != cudaSuccess) {
                std::cerr << "[MemoryManager] CUDA Error during initialization." << std::endl;
                return false;
            }

            all_slots_.push_back(slot);
            free_indices_.push_back(i);
        }
    } catch (...) {
        return false;
    }

    current_max_slots_ = max_slots;
    allowed_max_slots_ = max_slots;
    is_initialized_ = true;
    std::cout << "[MemoryManager] Initialized with " << max_slots << " slots." << std::endl;
    return true;
}

void MemoryManager::setBaseSlots(int new_base)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (new_base <= 0) return;
    base_slots_ = new_base;
}


Slot* MemoryManager::acquire() {
    std::unique_lock<std::mutex> lock(mutex_);

    // 等待时同时检查 shutdown_ 标志，避免永久阻塞
    cv_.wait(lock, [this] {
        if (shutdown_) return true;  // shutdown 后立即返回
        if (free_indices_.empty()) return false;
        for (int i : free_indices_) if (i < allowed_max_slots_) return true;
        return false;
    });

    // shutdown 后直接返回 nullptr
    if (shutdown_) return nullptr;
    int idx = -1;
    // 弹出第一个有效的索引
    while (!free_indices_.empty()) {
        int cand = free_indices_.front();
        free_indices_.pop_front();
        if (cand < allowed_max_slots_) {
            idx = cand;
            break;
        }
        // 否则丢弃该索引（它处于缩减范围之外）
    }

    if (idx < 0) return nullptr;
    // 更新峰值占用统计
    int used_now = allowed_max_slots_ - static_cast<int>(free_indices_.size());
    int prev_peak = peak_slots_used_.load(std::memory_order_relaxed);
    while (used_now > prev_peak && !peak_slots_used_.compare_exchange_weak(prev_peak, used_now, std::memory_order_relaxed));
    Slot* slot = all_slots_[idx];
    slot->clear();
    return slot;
}

Slot* MemoryManager::tryAcquire() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (shutdown_) return nullptr;

    int idx = -1;
    for (auto it = free_indices_.begin(); it != free_indices_.end(); ++it) {
        if (*it < allowed_max_slots_) {
            idx = *it;
            free_indices_.erase(it);
            break;
        }
    }
    if (idx < 0) return nullptr;

    int used_now = allowed_max_slots_ - static_cast<int>(free_indices_.size());
    int prev_peak = peak_slots_used_.load(std::memory_order_relaxed);
    while (used_now > prev_peak && !peak_slots_used_.compare_exchange_weak(prev_peak, used_now, std::memory_order_relaxed));

    Slot* slot = all_slots_[idx];
    slot->clear();
    return slot;
}

void MemoryManager::release(Slot* slot) {
    if (!slot) return;

    // 注意：release 动作本身不应该阻塞。
    // 在业务逻辑中，应在确认 cudaEventQuery(slot->event) == cudaSuccess 后再调用此函数。

    {
        std::lock_guard<std::mutex> lock(mutex_);
        // 仅当 slot id 在当前允许范围内时才放回队列
        if (slot->id_ < allowed_max_slots_) {
            free_indices_.push_back(slot->id_);
        } else {
            // 如果 slot 位于缩减范围之外，则不放回队列（保留已分配内存以便将来再次扩容）
        }
    }

    // 唤醒正在等待 acquire 的线程
    cv_.notify_one();
}

void MemoryManager::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    shutdown_ = true;
    cv_.notify_all(); // 唤醒所有卡在 acquire() 的线程
}

void MemoryManager::resetShutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    shutdown_ = false;
}

MemoryManager::~MemoryManager() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto slot : all_slots_) {
        if (slot) {
            if (slot->device_b_ptr_) cudaFree(slot->device_b_ptr_);
            if (slot->device_c_ptr_) cudaFree(slot->device_c_ptr_);
            if (slot->device_nv12_ptr_) cudaFree(slot->device_nv12_ptr_);
            if (slot->event_) cudaEventDestroy(slot->event_);
            delete slot;
        }
    }
    all_slots_.clear();
    free_indices_.clear();
    std::cout << "[MemoryManager] Resources released." << std::endl;
}

bool MemoryManager::adjustSlotsForMode(int grid_mode) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!is_initialized_) return false;

    (void)grid_mode;
    int desired = base_slots_;
    int slot_limit = getSlotLimit();
    if (desired > slot_limit) {
        std::cout << "[MemoryManager] Clamp desired slots: " << desired
                  << " -> " << slot_limit << " (limit)" << std::endl;
        desired = slot_limit;
    }

    if (desired <= allowed_max_slots_) {
        // 缩减可用数量（不释放内存，仅从可用队列中屏蔽高 id）
        allowed_max_slots_ = desired;
        // 移除 free_indices_ 中 id >= desired
        std::deque<int> tmp;
        for (int idx : free_indices_) if (idx < desired) tmp.push_back(idx);
        free_indices_.swap(tmp);
        std::cout << "[MemoryManager] Shrink allowed slots to " << desired << std::endl;
        return true;
    }

    // 扩容：分配新的 Slot
    for (int i = current_max_slots_; i < desired; ++i) {
        Slot* slot = new Slot();
        slot->id_ = i;
        slot->cur_batch_size_ = 0;
        slot->batch_capacity_ = samples_per_slot_;
        slot->metas_.clear();
        slot->sample_channel_ids_.clear();
        slot->sample_epochs_.clear();
        slot->stream_ = nullptr;
        slot->state_.store(Slot::State::Free);

        cudaError_t err_in = cudaMalloc(&slot->device_b_ptr_, input_tensor_bytes_);
        cudaError_t err_out = cudaMalloc(&slot->device_c_ptr_, output_tensor_bytes_);
        cudaError_t err_nv12 = cudaSuccess;
        if (nv12_bytes_ > 0) {
            err_nv12 = cudaMalloc(&slot->device_nv12_ptr_, nv12_bytes_ * static_cast<size_t>(samples_per_slot_));
            slot->nv12_w_ = nv12_w_;
            slot->nv12_h_ = nv12_h_;
            slot->nv12_pitch_ = nv12_w_;
        }
        cudaError_t err_ev = cudaEventCreateWithFlags(&slot->event_, cudaEventDisableTiming);
        if (err_in != cudaSuccess || err_out != cudaSuccess || err_nv12 != cudaSuccess || err_ev != cudaSuccess) {
            std::cerr << "[MemoryManager] CUDA Error during adjustSlotsForMode allocation." << std::endl;
            delete slot;
            return false;
        }

        all_slots_.push_back(slot);
        free_indices_.push_back(i);
    }

    current_max_slots_ = desired;
    allowed_max_slots_ = desired;
    std::cout << "[MemoryManager] Expanded slots to " << desired << std::endl;
    cv_.notify_all();
    return true;
}

bool MemoryManager::shrinkToBase()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!is_initialized_) return true;

    int keep = base_slots_;
    if (keep >= current_max_slots_) return true; // nothing to free

    // 检查是否所有待释放的 slot 都处于 free 状态
    std::unordered_set<int> freeSet;
    for (int idx : free_indices_) freeSet.insert(idx);
    for (int i = keep; i < current_max_slots_; ++i) {
        if (freeSet.find(i) == freeSet.end()) {
            std::cerr << "[MemoryManager] Cannot shrink: slot " << i << " is in use." << std::endl;
            return false;
        }
    }

    // 安全：释放这些 slot
    for (int i = current_max_slots_ - 1; i >= keep; --i) {
        Slot* s = all_slots_[i];
        if (s) {
            if (s->device_b_ptr_) { cudaFree(s->device_b_ptr_); s->device_b_ptr_ = nullptr; }
            if (s->device_c_ptr_) { cudaFree(s->device_c_ptr_); s->device_c_ptr_ = nullptr; }
            if (s->device_nv12_ptr_) { cudaFree(s->device_nv12_ptr_); s->device_nv12_ptr_ = nullptr; }
            if (s->event_) { cudaEventDestroy(s->event_); s->event_ = nullptr; }
            delete s;
        }
        all_slots_.pop_back();
    }

    // 重建 free_indices_: 移除 >= keep 的索引
    std::deque<int> tmp;
    for (int idx : free_indices_) if (idx < keep) tmp.push_back(idx);
    free_indices_.swap(tmp);

    current_max_slots_ = keep;
    allowed_max_slots_ = keep;
    std::cout << "[MemoryManager] Shrunk slots to base: " << keep << std::endl;
    return true;
}