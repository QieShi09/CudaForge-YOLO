#include "InputFrameArenaStore.hpp"

#include <algorithm>

InputFrameArenaStore::~InputFrameArenaStore() {
    shutdown();
}

bool InputFrameArenaStore::init(size_t arena_bytes, size_t sample_bytes, size_t max_ready_frames) {
    shutdown();
    if (arena_bytes == 0 || sample_bytes == 0 || max_ready_frames == 0) return false;

    if (!arena_.init(arena_bytes)) return false;

    std::lock_guard<std::mutex> lk(mutex_);
    frame_bytes_ = sample_bytes;
    max_ready_frames_ = max_ready_frames;
    dropped_frames_ = 0;
    enabled_ = true;
    disabled_channels_.clear();
    ready_queue_.clear();
    inflight_nodes_.clear();
    nodes_.clear();
    free_nodes_.clear();
    return true;
}

void InputFrameArenaStore::shutdown() {
    {
        std::lock_guard<std::mutex> lk(mutex_);
        enabled_ = false;
        ready_queue_.clear();
        inflight_nodes_.clear();
        for (auto& node : nodes_) {
            if (node && node->sample.ready_event) {
                cudaEventDestroy(node->sample.ready_event);
                node->sample.ready_event = nullptr;
            }
        }
        nodes_.clear();
        free_nodes_.clear();
        disabled_channels_.clear();
        frame_bytes_ = 0;
        max_ready_frames_ = 0;
    }
    cv_.notify_all();
    arena_.shutdown();
}

void InputFrameArenaStore::enable() {
    std::lock_guard<std::mutex> lk(mutex_);
    enabled_ = true;
}

void InputFrameArenaStore::disable() {
    std::lock_guard<std::mutex> lk(mutex_);
    enabled_ = false;
}

bool InputFrameArenaStore::isEnabled() const {
    std::lock_guard<std::mutex> lk(mutex_);
    return enabled_;
}

void InputFrameArenaStore::enableChannel(int channel_id) {
    std::lock_guard<std::mutex> lk(mutex_);
    disabled_channels_.erase(channel_id);
}

void InputFrameArenaStore::disableChannel(int channel_id) {
    std::lock_guard<std::mutex> lk(mutex_);
    disabled_channels_.insert(channel_id);
}

bool InputFrameArenaStore::isChannelEnabled(int channel_id) const {
    std::lock_guard<std::mutex> lk(mutex_);
    return enabled_ && disabled_channels_.find(channel_id) == disabled_channels_.end();
}

InputFrameArenaStore::Node* InputFrameArenaStore::acquireNodeLocked() {
    if (!free_nodes_.empty()) {
        Node* node = free_nodes_.back();
        free_nodes_.pop_back();
        return node;
    }

    std::unique_ptr<Node> holder(new Node());
    Node* node = holder.get();
    cudaEventCreateWithFlags(&node->sample.ready_event, cudaEventDisableTiming);
    nodes_.push_back(std::move(holder));
    return node;
}

void InputFrameArenaStore::recycleNodeLocked(Node* node) {
    if (!node) return;
    node->state = NodeState::Free;
    node->sample.ptr = nullptr;
    node->sample.bytes = 0;
    node->sample.channel_id = 0;
    node->sample.epoch = 0;
    node->sample.timestamp_us = 0;
    node->sample.preproc = Slot::PreprocMeta();
    node->sample.handle = nullptr;
    free_nodes_.push_back(node);
}

void InputFrameArenaStore::dropOldestReadyLocked() {
    if (ready_queue_.empty()) return;
    Node* node = ready_queue_.front();
    ready_queue_.pop_front();
    if (node && node->sample.ptr && node->sample.bytes > 0) {
        arena_.deallocate_after(node->sample.ptr, node->sample.bytes, node->sample.ready_event);
    }
    recycleNodeLocked(node);
    ++dropped_frames_;
}

bool InputFrameArenaStore::pushFrame(int channel_id,
                                     uint64_t epoch,
                                     int64_t timestamp_us,
                                     const Slot::PreprocMeta& preproc,
                                     cudaStream_t upload_stream,
                                     const std::function<bool(void* dst, size_t bytes, cudaStream_t stream)>& fill_fn) {
    if (!fill_fn) return false;

    {
        std::lock_guard<std::mutex> lk(mutex_);
        if (!enabled_) return false;
        if (disabled_channels_.find(channel_id) != disabled_channels_.end()) return false;
    }

    void* ptr = arena_.allocate(frame_bytes_, 256);
    if (!ptr) {
        std::lock_guard<std::mutex> lk(mutex_);
        ++dropped_frames_;
        return false;
    }

    if (!fill_fn(ptr, frame_bytes_, upload_stream)) {
        arena_.deallocate(ptr, frame_bytes_);
        std::lock_guard<std::mutex> lk(mutex_);
        ++dropped_frames_;
        return false;
    }

    Node* node = nullptr;
    {
        std::lock_guard<std::mutex> lk(mutex_);
        if (!enabled_ || disabled_channels_.find(channel_id) != disabled_channels_.end()) {
            arena_.deallocate(ptr, frame_bytes_);
            ++dropped_frames_;
            return false;
        }

        if (ready_queue_.size() >= max_ready_frames_) {
            dropOldestReadyLocked();
        }

        node = acquireNodeLocked();
        node->state = NodeState::Ready;
        node->sample.ptr = ptr;
        node->sample.bytes = frame_bytes_;
        node->sample.channel_id = channel_id;
        node->sample.epoch = epoch;
        node->sample.timestamp_us = timestamp_us;
        node->sample.preproc = preproc;
        node->sample.handle = node;

        ready_queue_.push_back(node);
    }

    if (cudaEventRecord(node->sample.ready_event, upload_stream) != cudaSuccess) {
        std::lock_guard<std::mutex> lk(mutex_);
        if (!ready_queue_.empty() && ready_queue_.back() == node) {
            ready_queue_.pop_back();
        }
        if (node->sample.ptr && node->sample.bytes > 0) {
            arena_.deallocate(node->sample.ptr, node->sample.bytes);
        }
        recycleNodeLocked(node);
        ++dropped_frames_;
        return false;
    }
    cv_.notify_one();
    return true;
}

std::vector<InputFrameArenaStore::FrameSample> InputFrameArenaStore::popBatch(size_t max_batch,
                                                                               size_t min_batch,
                                                                               std::chrono::milliseconds wait_ms) {
    std::vector<FrameSample> out;
    if (max_batch == 0) return out;

    std::unique_lock<std::mutex> lk(mutex_);
    auto enough = [this, min_batch]() {
        return !enabled_ || ready_queue_.size() >= std::max<size_t>(1, min_batch);
    };

    if (!enough()) {
        cv_.wait_for(lk, wait_ms, enough);
    }

    if (!enabled_ || ready_queue_.empty()) return out;

    size_t n = std::min(max_batch, ready_queue_.size());
    out.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        Node* node = ready_queue_.front();
        ready_queue_.pop_front();
        if (!node) continue;
        node->state = NodeState::Inflight;
        inflight_nodes_.insert(node);
        out.push_back(node->sample);
    }
    return out;
}

void InputFrameArenaStore::releaseBatchNow(const std::vector<FrameSample>& batch) {
    std::lock_guard<std::mutex> lk(mutex_);
    for (const auto& sample : batch) {
        Node* node = static_cast<Node*>(sample.handle);
        if (!node) continue;
        inflight_nodes_.erase(node);
        if (sample.ptr && sample.bytes > 0) {
            arena_.deallocate(sample.ptr, sample.bytes);
        }
        recycleNodeLocked(node);
    }
}

void InputFrameArenaStore::releaseBatchAfter(const std::vector<FrameSample>& batch, cudaEvent_t event) {
    std::lock_guard<std::mutex> lk(mutex_);
    for (const auto& sample : batch) {
        Node* node = static_cast<Node*>(sample.handle);
        if (!node) continue;
        inflight_nodes_.erase(node);
        if (sample.ptr && sample.bytes > 0) {
            arena_.deallocate_after(sample.ptr, sample.bytes, event);
        }
        recycleNodeLocked(node);
    }
}

void InputFrameArenaStore::clearChannel(int channel_id) {
    std::lock_guard<std::mutex> lk(mutex_);
    for (auto it = ready_queue_.begin(); it != ready_queue_.end();) {
        Node* node = *it;
        if (node && node->sample.channel_id == channel_id) {
            if (node->sample.ptr && node->sample.bytes > 0) {
                arena_.deallocate_after(node->sample.ptr, node->sample.bytes, node->sample.ready_event);
            }
            recycleNodeLocked(node);
            it = ready_queue_.erase(it);
        } else {
            ++it;
        }
    }
}

InputFrameArenaStore::Stats InputFrameArenaStore::getStats() const {
    std::lock_guard<std::mutex> lk(mutex_);
    Stats s;
    s.ready_frames = ready_queue_.size();
    s.inflight_frames = inflight_nodes_.size();
    s.max_ready_frames = max_ready_frames_;
    s.dropped_frames = dropped_frames_;
    s.arena = arena_.getStats();
    return s;
}

GpuArena::Stats InputFrameArenaStore::arenaStats() const {
    return arena_.getStats();
}

std::vector<InputFrameArenaStore::SampleRange> InputFrameArenaStore::getSampleRanges() const {
    std::vector<SampleRange> ranges;
    std::lock_guard<std::mutex> lk(mutex_);
    ranges.reserve(nodes_.size());
    for (const auto& n : nodes_) {
        if (!n || n->state == NodeState::Free || n->sample.ptr == nullptr || n->sample.bytes == 0) {
            continue;
        }
        SampleRange range;
        range.ptr = reinterpret_cast<uintptr_t>(n->sample.ptr);
        range.bytes = n->sample.bytes;
        range.state = (n->state == NodeState::Ready) ? SampleState::Ready : SampleState::Inflight;
        ranges.push_back(range);
    }
    return ranges;
}

uintptr_t InputFrameArenaStore::arenaBaseAddress() const {
    return arena_.baseAddress();
}
