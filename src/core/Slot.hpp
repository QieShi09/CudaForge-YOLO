#ifndef SLOT_HPP
#define SLOT_HPP

#include <vector>
#include <cstdint>
#include <cstddef>
#include <atomic>
#include <functional>
#include <cuda_runtime_api.h>

/**
 * @brief Slot：管理GPU计算内存对和进度锚点
 */
class Slot {
    friend class MemoryManager;
public:
    struct PreprocMeta {
        int orig_w = 0;
        int orig_h = 0;
        int roi_x = 0;
        int roi_y = 0;
        int roi_w = 0;
        int roi_h = 0;
        float scale = 1.0f; // 缩放比例（用于从模型坐标映射回原图）
        int pad_w = 0;      // letterbox 填充宽度
        int pad_h = 0;      // letterbox 填充高度
        bool hist_eq = false; // 是否做了直方图均衡
    };

    enum class State : int {
        Empty = 0,
        Preprocessed,
        Queued,
        Inferring,
        Postprocessed
    };

    // 构造函数设为私有，禁止外部直接 new Slot

    // 批量塞入接口：只写入元数据，不在 Slot 内启动内核
    void setupBatch(const std::vector<int>& ch_ids,
                    const std::vector<int64_t>& ts,
                    const std::vector<int>& src_formats,
                    const std::vector<int>& detection_steps,
                    cudaStream_t stream)
    {
        cur_batch_size_ = static_cast<int>(ch_ids.size());
        channel_ids_ = ch_ids;
        timestamps_ = ts;
        src_formats_ = src_formats;
        detection_steps_ = detection_steps;
        stream_ = stream;
        state_.store(State::Empty);
    }

    // --- 基本访问器 ---
    int getId() const { return id_; }
    int getCurBatchSize() const { return cur_batch_size_; }
    void* getDeviceIn() { return device_b_ptr_; }
    void* getDeviceOut() { return device_c_ptr_; }
    cudaEvent_t getEvent() { return event_; }
    cudaStream_t getStream() const { return stream_; }

    // 预处理元数据管理
    void setPreprocMeta(int idx, const PreprocMeta& m) {
        if (idx < 0) return;
        if (static_cast<size_t>(idx) >= metas_.size()) metas_.resize(idx + 1);
        metas_[idx] = m;
    }
    PreprocMeta getPreprocMeta(int idx) const {
        if (idx < 0 || static_cast<size_t>(idx) >= metas_.size()) return PreprocMeta();
        return metas_[idx];
    }

    // 状态与回调
    void markState(State s) { state_.store(s); }
    State getState() const { return state_.load(); }
    void setOnInferDone(std::function<void(Slot&)> cb) { on_infer_done_ = std::move(cb); }
    void callOnInferDone() { if (on_infer_done_) on_infer_done_(*this); }

    // 清理函数：还回池子前重置元数据
    void clear() {
        channel_ids_.clear();
        timestamps_.clear();
        src_formats_.clear();
        detection_steps_.clear();
        metas_.clear();
        assigned_stream_ids_.clear();
        cur_batch_size_ = 0;
        state_.store(State::Empty);
        on_infer_done_ = nullptr;
        stream_ = nullptr;
    }

private:
    Slot() = default;
    int id_ = -1;
    int cur_batch_size_ = 0;

    void* device_b_ptr_ = nullptr; // 连续的输入Tensor空间 (NCHW)
    void* device_c_ptr_ = nullptr; // 连续的输出结果空间
    cudaEvent_t event_  = nullptr; // 锚定进度的Event

    std::vector<int> channel_ids_;
    std::vector<int64_t> timestamps_;

    // 每样本的预处理元数据（用于映射回原图）
    std::vector<PreprocMeta> metas_;

    // 多流分配信息
    std::vector<int> assigned_stream_ids_;
    cudaStream_t stream_ = nullptr; // 用于本 Slot 的 CUDA stream（非必需但方便追踪）

    // 每样本的源格式与检测步长
    std::vector<int> src_formats_;
    std::vector<int> detection_steps_;

    std::atomic<State> state_{State::Empty};

    // 推理完成回调（可用于唤醒上层队列/合并逻辑）
    std::function<void(Slot&)> on_infer_done_;
};

#endif
