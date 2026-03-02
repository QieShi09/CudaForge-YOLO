#ifndef DETECTION_RESULTS_HPP
#define DETECTION_RESULTS_HPP

#include <unordered_map>
#include <vector>
#include <mutex>
#include <chrono>
#include <cstdint>

class DetectionResults {
public:
    struct DetectionBox {
        float x = 0.0f;
        float y = 0.0f;
        float w = 0.0f;
        float h = 0.0f;
        float conf = 0.0f;
        int class_id = 0;
    };

    // 每个通道每个类别的累计统计
    struct ClassStats {
        int count = 0;          // 检出次数（累计每帧中该类别的目标数量）
        float max_conf = 0.0f;  // 最大置信度
    };

    // 每个通道的累计统计
    struct ChannelStats {
        int total_frames = 0;   // 推理帧数
        std::unordered_map<int, ClassStats> class_stats; // class_id -> stats
    };

    static DetectionResults& getInstance() {
        static DetectionResults inst;
        return inst;
    }

    // ========== Epoch 机制 ==========
    // 每次通道重启（startChannel）时递增 epoch，
    // Worker 回调仅在 epoch 匹配时写入，防止旧的异步推理回调覆盖新结果。
    uint64_t bumpEpoch(int channel_id) {
        std::lock_guard<std::mutex> lk(m_);
        return ++epochs_[channel_id];
    }

    uint64_t getEpoch(int channel_id) {
        std::lock_guard<std::mutex> lk(m_);
        return epochs_[channel_id]; // 默认 0
    }

    // 仅当 epoch 匹配时才写入（用于 Worker 回调）
    bool updateIfCurrent(int channel_id, uint64_t epoch, std::vector<DetectionBox> dets) {
        std::lock_guard<std::mutex> lk(m_);
        if (epochs_[channel_id] != epoch) return false; // 旧回调，丢弃
        results_[channel_id] = dets;
        // 累积统计
        auto& cs = accumulated_[channel_id];
        cs.total_frames++;
        for (const auto& d : dets) {
            auto& s = cs.class_stats[d.class_id];
            s.count++;
            if (d.conf > s.max_conf) s.max_conf = d.conf;
        }
        return true;
    }

    // 无条件写入（向后兼容，视频连续流可用）
    void update(int channel_id, std::vector<DetectionBox> dets) {
        std::lock_guard<std::mutex> lk(m_);
        results_[channel_id] = dets;
        auto& cs = accumulated_[channel_id];
        cs.total_frames++;
        for (const auto& d : dets) {
            auto& s = cs.class_stats[d.class_id];
            s.count++;
            if (d.conf > s.max_conf) s.max_conf = d.conf;
        }
    }

    std::vector<DetectionBox> get(int channel_id) {
        std::lock_guard<std::mutex> lk(m_);
        auto it = results_.find(channel_id);
        if (it == results_.end()) return {};
        return it->second;
    }

    // 获取累计统计（用于报告生成）
    std::unordered_map<int, ChannelStats> getAccumulated() {
        std::lock_guard<std::mutex> lk(m_);
        return accumulated_;
    }

    void clear(int channel_id) {
        std::lock_guard<std::mutex> lk(m_);
        results_.erase(channel_id);
        // 注意：不清除累计统计，它们由 resetAccumulated 控制
    }

    void clearAll() {
        std::lock_guard<std::mutex> lk(m_);
        results_.clear();
    }

    // 重置累计统计（新检测会话开始时调用）
    void resetAccumulated() {
        std::lock_guard<std::mutex> lk(m_);
        accumulated_.clear();
    }

private:
    DetectionResults() = default;
    ~DetectionResults() = default;
    DetectionResults(const DetectionResults&) = delete;
    DetectionResults& operator=(const DetectionResults&) = delete;

    std::unordered_map<int, std::vector<DetectionBox>> results_;
    std::unordered_map<int, ChannelStats> accumulated_;
    std::unordered_map<int, uint64_t> epochs_;  // 通道 epoch 计数器
    std::mutex m_;
};

#endif // DETECTION_RESULTS_HPP
