#ifndef INPUT_FRAME_ARENA_STORE_HPP
#define INPUT_FRAME_ARENA_STORE_HPP

#include "GpuArena.hpp"
#include "Slot.hpp"
#include <cuda_runtime_api.h>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <unordered_set>
#include <unordered_map>
#include <functional>
#include <chrono>

class InputFrameArenaStore {
public:
    struct FrameSample {
        void* ptr = nullptr;
        size_t bytes = 0;
        int channel_id = 0;
        uint64_t epoch = 0;
        int64_t timestamp_us = 0;
        Slot::PreprocMeta preproc;
        cudaEvent_t ready_event = nullptr;
        void* handle = nullptr;
    };

    struct Stats {
        size_t ready_frames = 0;
        size_t inflight_frames = 0;
        size_t max_ready_frames = 0;
        uint64_t dropped_frames = 0;
        GpuArena::Stats arena;
    };

    static InputFrameArenaStore& getInstance() {
        static InputFrameArenaStore inst;
        return inst;
    }

    bool init(size_t arena_bytes, size_t frame_bytes, size_t frame_width, size_t max_ready_frames);
    void shutdown();

    void enable();
    void disable();
    bool isEnabled() const;

    void enableChannel(int channel_id);
    void disableChannel(int channel_id);
    bool isChannelEnabled(int channel_id) const;
    void clearChannel(int channel_id);

    bool pushFrame(int channel_id,
                   uint64_t epoch,
                   int64_t timestamp_us,
                   const Slot::PreprocMeta& preproc,
                   cudaStream_t upload_stream,
                   const std::function<bool(uint8_t* dst_y, uint8_t* dst_uv, int pitch, cudaStream_t stream)>& fill_fn);

    std::vector<FrameSample> popBatch(size_t max_batch,
                                      size_t min_batch,
                                      std::chrono::milliseconds wait_ms);

    void releaseBatchNow(const std::vector<FrameSample>& batch);
    void releaseBatchAfter(const std::vector<FrameSample>& batch, cudaEvent_t event);

    Stats getStats() const;
    GpuArena::Stats arenaStats() const;

private:
    enum class NodeState : uint8_t { Free = 0, Ready = 1, Inflight = 2 };

    struct Node {
        FrameSample sample;
        NodeState state = NodeState::Free;
    };

    InputFrameArenaStore() = default;
    ~InputFrameArenaStore();
    InputFrameArenaStore(const InputFrameArenaStore&) = delete;
    InputFrameArenaStore& operator=(const InputFrameArenaStore&) = delete;

    Node* acquireNodeLocked();
    void recycleNodeLocked(Node* node);
    void dropOldestReadyLocked();

    mutable std::mutex mutex_;
    std::condition_variable cv_;

    GpuArena arena_{"input-frame-arena"};
    size_t frame_bytes_ = 0;
    size_t frame_pitch_ = 0;
    size_t frame_width_ = 0;
    size_t max_ready_frames_ = 0;

    std::deque<Node*> ready_queue_;
    std::unordered_set<Node*> inflight_nodes_;
    std::vector<std::unique_ptr<Node>> nodes_;
    std::vector<Node*> free_nodes_;

    std::unordered_set<int> disabled_channels_;
    bool enabled_ = false;

    uint64_t dropped_frames_ = 0;
};

#endif
