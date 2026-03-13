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
    friend class SlotPool;
public:
    struct PreprocMeta {
        int orig_w = 0;
        int orig_h = 0;
        float scale = 1.0f; // 缩放比例（用于从模型坐标映射回原图）
        int pad_w = 0;      // letterbox 填充宽度
        int pad_h = 0;      // letterbox 填充高度
    };

    struct FrameMeta {
        int64_t timestamp_us = 0;
        int channel_id = 0;
        uint64_t epoch = 0;
        PreprocMeta preproc;
    };

    enum class State : int {
        Free = 0,
        Ready,
        InUse
    };

    // 构造函数设为私有，禁止外部直接 new Slot

    // 批量塞入接口：只写入元数据，不在 Slot 内启动内核
    void setupBatch(int batch_size, cudaStream_t stream)
    {
        cur_batch_size_ = (batch_size < 0) ? 0 : ((batch_size > batch_capacity_) ? batch_capacity_ : batch_size);
        stream_ = stream;
        state_.store(State::InUse);
    }

    // --- 基本访问器 ---
    int getId() const { return id_; }
    int getCurBatchSize() const { return cur_batch_size_; }
    int getBatchCapacity() const { return batch_capacity_; }
    void* getDeviceIn() { return device_b_ptr_; }
    void* getDeviceOut() { return device_c_ptr_; }
    void setDeviceIn(void* ptr, size_t bytes) { device_b_ptr_ = ptr; in_bytes_ = bytes; }
    void setDeviceOut(void* ptr, size_t bytes) { device_c_ptr_ = ptr; out_bytes_ = bytes; }
    size_t getInputBytes() const { return in_bytes_; }
    size_t getOutputBytes() const { return out_bytes_; }
    void* getDeviceNV12() { return device_nv12_ptr_; }
    size_t getNV12FrameBytes() const {
        return (nv12_w_ > 0 && nv12_h_ > 0) ? (static_cast<size_t>(nv12_w_) * nv12_h_ * 3 / 2) : 0;
    }
    uint8_t* getDeviceNV12Y(int idx = 0) {
        size_t frame_bytes = getNV12FrameBytes();
        if (!device_nv12_ptr_ || frame_bytes == 0 || idx < 0) return nullptr;
        return reinterpret_cast<uint8_t*>(device_nv12_ptr_) + static_cast<size_t>(idx) * frame_bytes;
    }
    uint8_t* getDeviceNV12UV(int idx = 0) {
        if (!device_nv12_ptr_ || nv12_w_ <= 0 || nv12_h_ <= 0) return nullptr;
        size_t frame_bytes = getNV12FrameBytes();
        if (frame_bytes == 0 || idx < 0) return nullptr;
        return reinterpret_cast<uint8_t*>(device_nv12_ptr_) + static_cast<size_t>(idx) * frame_bytes + static_cast<size_t>(nv12_w_) * nv12_h_;
    }
    int getNV12W() const { return nv12_w_; }
    int getNV12H() const { return nv12_h_; }
    int getNV12Pitch() const { return nv12_pitch_; }
    cudaEvent_t getEvent() { return event_; }
    cudaStream_t getStream() const { return stream_; }
    bool canAppendSample() const { return cur_batch_size_ < batch_capacity_; }
    int getNextSampleIndex() const { return cur_batch_size_; }
    bool appendSampleMeta(int channel_id, uint64_t epoch, const PreprocMeta& m) {
        if (cur_batch_size_ >= batch_capacity_) return false;
        const size_t idx = static_cast<size_t>(cur_batch_size_);
        if (metas_.size() <= idx) metas_.resize(idx + 1);
        if (sample_channel_ids_.size() <= idx) sample_channel_ids_.resize(idx + 1);
        if (sample_epochs_.size() <= idx) sample_epochs_.resize(idx + 1);
        metas_[idx] = m;
        sample_channel_ids_[idx] = channel_id;
        sample_epochs_[idx] = epoch;
        if (frame_metas_.size() <= idx) frame_metas_.resize(idx + 1);
        frame_metas_[idx].channel_id = channel_id;
        frame_metas_[idx].epoch = epoch;
        frame_metas_[idx].preproc = m;
        ++cur_batch_size_;
        state_.store(State::Ready);
        return true;
    }

    void setFrameTimestamp(int idx, int64_t timestamp_us) {
        if (idx < 0) return;
        if (static_cast<size_t>(idx) >= frame_metas_.size()) frame_metas_.resize(idx + 1);
        frame_metas_[static_cast<size_t>(idx)].timestamp_us = timestamp_us;
    }
    FrameMeta getFrameMeta(int idx) const {
        if (idx < 0 || static_cast<size_t>(idx) >= frame_metas_.size()) return FrameMeta();
        return frame_metas_[static_cast<size_t>(idx)];
    }

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
    int getSampleChannelId(int idx) const {
        if (idx < 0 || static_cast<size_t>(idx) >= sample_channel_ids_.size()) return 0;
        return sample_channel_ids_[idx];
    }
    uint64_t getSampleEpoch(int idx) const {
        if (idx < 0 || static_cast<size_t>(idx) >= sample_epochs_.size()) return 0;
        return sample_epochs_[idx];
    }

    // 状态与回调
    void markState(State s) { state_.store(s); }
    State getState() const { return state_.load(); }
    void setOnInferDone(std::function<void(Slot&)> cb) { on_infer_done_ = std::move(cb); }
    void callOnInferDone() { if (on_infer_done_) on_infer_done_(*this); }

    // 清理函数：还回池子前重置元数据
    void clear() {
        metas_.clear();
        sample_channel_ids_.clear();
        sample_epochs_.clear();
        frame_metas_.clear();
        cur_batch_size_ = 0;
        state_.store(State::Free);
        on_infer_done_ = nullptr;
        stream_ = nullptr;
        in_bytes_ = 0;
        out_bytes_ = 0;
        device_b_ptr_ = nullptr;
        device_c_ptr_ = nullptr;
    }

private:
    Slot() = default;
    int id_ = -1;
    int cur_batch_size_ = 0;
    int batch_capacity_ = 1;

    void* device_b_ptr_ = nullptr; // 连续的输入Tensor空间 (NCHW)
    void* device_c_ptr_ = nullptr; // 连续的输出结果空间
    size_t in_bytes_ = 0;
    size_t out_bytes_ = 0;
    void* device_nv12_ptr_ = nullptr; // letterbox 后的 NV12 缓冲
    int nv12_w_ = 0;
    int nv12_h_ = 0;
    int nv12_pitch_ = 0;
    cudaEvent_t event_  = nullptr; // 锚定进度的Event

    // 每样本的预处理元数据（用于映射回原图）
    std::vector<PreprocMeta> metas_;
    std::vector<int> sample_channel_ids_;
    std::vector<uint64_t> sample_epochs_;
    std::vector<FrameMeta> frame_metas_;

    cudaStream_t stream_ = nullptr; // 用于本 Slot 的 CUDA stream（非必需但方便追踪）

    std::atomic<State> state_{State::Free};

    // 推理完成回调（可用于唤醒上层队列/合并逻辑）
    std::function<void(Slot&)> on_infer_done_;
};

#endif
