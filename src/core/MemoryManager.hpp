#ifndef MEMORY_MANAGER_HPP
#define MEMORY_MANAGER_HPP

#include "Slot.hpp"
#include <vector>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <memory>
#include <iostream>
#include <atomic>

/**
 * @brief MemoryManager (单例)
 * 职责：预分配 Slot 资源池，提供线程安全的申请与归还接口。
 * 注意：该类仅管理 Slot 内部推理所需的“固定连续显存”，不管理 FFmpeg 的原始显存。
 */
class MemoryManager {
public:
    // 获取单例句柄
    static MemoryManager& getInstance() {
        static MemoryManager instance;
        return instance;
    }

    /**
     * @brief 初始化内存池
     * @param max_slots 队列中允许存在的最大 Slot 数量（建议根据路数和 Batch 大小平衡，如 5~10）
     * @param input_tensor_bytes  单个 Batch 输入 Tensor 的字节数 (Batch * C * H * W * sizeof(float))
     * @param output_tensor_bytes 单个 Batch 输出结果的字节数
     * @return 是否初始化成功
     */
    bool init(int max_slots, size_t input_tensor_bytes, size_t output_tensor_bytes);

    /**
     * @brief 热更新基础 slot 数量（影响后续 adjustSlotsForMode / shrinkToBase）
     */
    void setBaseSlots(int new_base);

    /**
     * @brief 根据模式动态调整 slot 数量（仅增量分配或临时收缩）
     * @param grid_mode 1 表示 1x1 (1 stream)，2 表示 2x2 (2 streams)，3 表示 3x3 (4 streams)
     */
    bool adjustSlotsForMode(int grid_mode);

    /**
     * @brief 阻塞式获取一个可用 Slot
     * 如果池子空了，当前调用线程会挂起，直到其他线程调用 release。
     * 如果队列已 shutdown，返回 nullptr。
     * @return Slot* 指针，或 nullptr（shutdown 后）
     */
    Slot* acquire();

    /**
     * @brief 归还 Slot 到池子
     * 调用该函数前，请确保该 Slot 的所有异步任务（推理等）已经通过 Event 确认完成。
     * @param slot 需要回收的 Slot 指针
     */
    void release(Slot* slot);

    /**
     * @brief 停止内存池，唤醒所有卡在 acquire() 的线程
     * 后续的 acquire() 调用将立即返回 nullptr
     */
    void shutdown();

    /**
     * @brief 重置 shutdown 状态（用于重新启动）
     */
    void resetShutdown();

    /**
     * @brief 当前空闲可用的 Slot 数量（用于监控）
     */
    int availableSlots() const {
        std::lock_guard<std::mutex> lock(mutex_);
        int count = 0;
        for (int i : free_indices_) if (i < allowed_max_slots_) ++count;
        return count;
    }

    /**
     * @brief 当前允许使用的 Slot 上限（用于监控）
     */
    int totalSlots() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return allowed_max_slots_;
    }

    /**
     * @brief 每个 Slot 估算显存占用（输入+输出 Tensor 总字节数）
     */
    size_t slotBytesPerSlot() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return input_tensor_bytes_ + output_tensor_bytes_;
    }

    /**
     * @brief 已物理分配的 Slot 显存总量（字节）
     */
    size_t totalSlotBytesAllocated() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return static_cast<size_t>(current_max_slots_) * (input_tensor_bytes_ + output_tensor_bytes_);
    }

    /**
     * @brief 当前允许使用的 Slot 显存总量（字节）
     */
    size_t allowedSlotBytes() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return static_cast<size_t>(allowed_max_slots_) * (input_tensor_bytes_ + output_tensor_bytes_);
    }

    /**
     * @brief 自上次重置以来的峰值 Slot 占用数（用于监控）
     */
    int peakSlotsUsed() const {
        return peak_slots_used_.load(std::memory_order_relaxed);
    }

    /**
     * @brief 重置峰值计数器
     */
    void resetPeakSlots() {
        peak_slots_used_.store(0, std::memory_order_relaxed);
    }

    /**
     * @brief 立即释放在 allowed_max_slots_ 之外分配的物理 Slot（真正释放显存）
     * 如果存在尚在使用的 slot，则不会执行释放。
     * @return true 表示成功释放或无须释放，false 表示检测到正在使用的 slot，未释放
     */
    bool shrinkToBase();

    /**
     * @brief 是否已初始化
     */
    bool isInitialized() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return is_initialized_;
    }

    // 禁止拷贝与赋值
    MemoryManager(const MemoryManager&) = delete;
    MemoryManager& operator=(const MemoryManager&) = delete;

private:
    MemoryManager() = default;
    ~MemoryManager();

    std::vector<Slot*> all_slots_;      // 物理持有的所有 Slot 对象（按 id 顺序）
    std::deque<int> free_indices_;      // 空闲 Slot 的索引队列

    mutable std::mutex mutex_;                  // 保护 free_indices_ 的互斥锁
    std::condition_variable cv_;        // 用于实现 acquire 的阻塞等待

    bool is_initialized_ = false;       // 初始化状态位

    // 动态调整相关
    int base_slots_ = 0;                // init 时的基准 slot 数
    int current_max_slots_ = 0;         // 已分配的 slot 数量（all_slots_.size())
    int allowed_max_slots_ = 0;         // 当前允许被 acquire 的 slot 上限（<= current_max_slots_）
    bool shutdown_ = false;             // 停止标志，用于唤醒 acquire() 阻塞线程
    size_t input_tensor_bytes_ = 0;
    size_t output_tensor_bytes_ = 0;
    std::atomic<int> peak_slots_used_{0};  // 峰值 Slot 占用数
    // （不再在 MemoryManager 管理 stream 池，stream 由 Worker 管理或按需创建）
};

#endif
