#ifndef PIPELINE_STATS_HPP
#define PIPELINE_STATS_HPP

#include <atomic>
#include <cstdint>

/**
 * @brief 全局管道吞吐量计数器（原子操作，线程安全）
 * 各模块递增自己的计数器，监控定时器定期读取并重置。
 */
struct PipelineStats {
    static PipelineStats& getInstance() {
        static PipelineStats instance;
        return instance;
    }

    // === 解码阶段 ===
    std::atomic<uint64_t> frames_decoded{0};      // 解码成功的帧数
    std::atomic<uint64_t> frames_pushed_dq{0};    // 成功推入 DetectionQueue 的帧数
    std::atomic<uint64_t> frames_dropped_dq{0};   // DetectionQueue 满溢丢弃的帧数
    std::atomic<uint64_t> demux_packets_read{0};  // Demux 读取到的视频包数
    std::atomic<uint64_t> demux_read_us{0};       // av_read_frame 总耗时（微秒）
    std::atomic<uint64_t> packets_popped{0};      // PacketQueue 弹出的包数
    std::atomic<uint64_t> decode_pop_wait_us{0};  // PacketQueue pop 等待总耗时（微秒）
    std::atomic<uint64_t> decode_send_us{0};      // avcodec_send_packet 总耗时（微秒）
    std::atomic<uint64_t> decode_receive_us{0};   // avcodec_receive_frame 总耗时（微秒）
    std::atomic<uint64_t> decode_upload_us{0};    // CPU->GPU upload 总耗时（微秒）
    std::atomic<uint64_t> frames_uploaded{0};     // CPU->GPU upload 的帧数
    std::atomic<uint64_t> framequeue_push_wait_us{0}; // FrameQueue push 等待总耗时（微秒）
    std::atomic<uint64_t> frames_pushed_fq{0};    // 成功推入 FrameQueue 的帧数
    std::atomic<uint64_t> frames_dropped_fq{0};   // FrameQueue 推入失败丢弃帧数

    // === 推理阶段 ===
    std::atomic<uint64_t> batches_inferred{0};    // Worker 提交的推理批次数
    std::atomic<uint64_t> frames_inferred{0};     // Worker 处理的帧数（含批内）
    std::atomic<uint64_t> detections_total{0};    // 检测到的目标总数

    // === 显示阶段 ===
    std::atomic<uint64_t> frames_displayed{0};    // DisplayWorker 处理的帧数

    // === Worker 效率指标 ===
    std::atomic<uint64_t> worker_pop_empty{0};       // pop_bulk 返回空（Worker 在等数据）
    std::atomic<uint64_t> worker_batches_popped{0};  // pop_bulk 成功次数
    std::atomic<uint64_t> worker_frames_popped{0};   // pop_bulk 总帧数（用于计算 avg batch size）
    std::atomic<uint64_t> worker_slot_wait_us{0};    // 累计 slot acquire 等待时间（微秒）
    std::atomic<uint64_t> worker_preproc_us{0};      // 累计预处理耗时（微秒）
    std::atomic<uint64_t> worker_gpu_preproc_us{0};  // GPU 预处理耗时（CUDA Event，微秒）
    std::atomic<uint64_t> worker_gpu_infer_us{0};    // GPU 推理耗时（CUDA Event，微秒）
    std::atomic<uint64_t> worker_gpu_batches{0};     // GPU 计时批次数

    // === 后处理阶段 ===
    std::atomic<uint64_t> postprocess_d2h_us{0};     // DtoH 结果拷贝耗时（微秒）
    std::atomic<uint64_t> postprocess_batches{0};    // parseDetections 批次数
    std::atomic<uint64_t> postprocess_frames{0};     // parseDetections 帧数

    // === TRT Context Pool ===
    std::atomic<uint64_t> ctx_pool_hits{0};          // 从池中获取 context 成功
    std::atomic<uint64_t> ctx_pool_misses{0};        // 池空，需新建 context
    std::atomic<int>      ctx_pool_size{0};          // 当前池中闲置 context 数

    void resetAll() {
        frames_decoded.store(0, std::memory_order_relaxed);
        frames_pushed_dq.store(0, std::memory_order_relaxed);
        frames_dropped_dq.store(0, std::memory_order_relaxed);
        demux_packets_read.store(0, std::memory_order_relaxed);
        demux_read_us.store(0, std::memory_order_relaxed);
        packets_popped.store(0, std::memory_order_relaxed);
        decode_pop_wait_us.store(0, std::memory_order_relaxed);
        decode_send_us.store(0, std::memory_order_relaxed);
        decode_receive_us.store(0, std::memory_order_relaxed);
        decode_upload_us.store(0, std::memory_order_relaxed);
        frames_uploaded.store(0, std::memory_order_relaxed);
        framequeue_push_wait_us.store(0, std::memory_order_relaxed);
        frames_pushed_fq.store(0, std::memory_order_relaxed);
        frames_dropped_fq.store(0, std::memory_order_relaxed);
        batches_inferred.store(0, std::memory_order_relaxed);
        frames_inferred.store(0, std::memory_order_relaxed);
        detections_total.store(0, std::memory_order_relaxed);
        frames_displayed.store(0, std::memory_order_relaxed);
        worker_pop_empty.store(0, std::memory_order_relaxed);
        worker_batches_popped.store(0, std::memory_order_relaxed);
        worker_frames_popped.store(0, std::memory_order_relaxed);
        worker_slot_wait_us.store(0, std::memory_order_relaxed);
        worker_preproc_us.store(0, std::memory_order_relaxed);
        worker_gpu_preproc_us.store(0, std::memory_order_relaxed);
        worker_gpu_infer_us.store(0, std::memory_order_relaxed);
        worker_gpu_batches.store(0, std::memory_order_relaxed);
        postprocess_d2h_us.store(0, std::memory_order_relaxed);
        postprocess_batches.store(0, std::memory_order_relaxed);
        postprocess_frames.store(0, std::memory_order_relaxed);
        ctx_pool_hits.store(0, std::memory_order_relaxed);
        ctx_pool_misses.store(0, std::memory_order_relaxed);
    }

private:
    PipelineStats() = default;
};

#endif // PIPELINE_STATS_HPP
