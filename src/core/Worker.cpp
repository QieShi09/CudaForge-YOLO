#include "Worker.hpp"
#include "SlotQueue.hpp"
#include "MemoryManager.hpp"
#include "TRTDetector.hpp"
#include "Slot.hpp"
#include "DetectionResults.hpp"
#include "PipelineStats.hpp"
#include "NvtxUtils.hpp"
#include <cuda_runtime_api.h>
#include "../kernels/CudaImageProc.cuh"
#include <iostream>
#include <cstdlib>
#include <algorithm>
#include <thread>

Worker::Worker(int id, size_t max_batch, std::chrono::milliseconds max_wait_ms,
                             int total_workers, int stream_count)
        : id_(id),
            max_batch_(std::max<size_t>(1, max_batch)),
            max_wait_ms_(max_wait_ms),
            total_workers_(total_workers),
            stream_count_(std::max(1, stream_count)) {}

Worker::~Worker() { stop(); }

void Worker::start() {
    if (running_.exchange(true)) return;
    cudaSetDevice(0);
    streams_.clear();
    streams_.reserve(static_cast<size_t>(stream_count_));
    for (int i = 0; i < stream_count_; ++i) {
        cudaStream_t stream = nullptr;
        if (cudaStreamCreateWithFlags(&stream, cudaStreamNonBlocking) == cudaSuccess && stream) {
            streams_.push_back(stream);
        }
    }
    if (streams_.empty()) {
        cudaStream_t fallback = nullptr;
        cudaStreamCreateWithFlags(&fallback, cudaStreamNonBlocking);
        if (fallback) streams_.push_back(fallback);
    }
    stream_rr_ = 0;
    thr_ = std::thread(&Worker::run, this);
}

void Worker::stop() {
    if (!running_.exchange(false)) return;
    if (thr_.joinable()) thr_.join();
    for (auto& stream : streams_) {
        if (stream) cudaStreamDestroy(stream);
    }
    streams_.clear();
}

void Worker::run() {
    // 确保 worker 线程绑定正确的 CUDA 设备
    cudaSetDevice(0);

    auto pick_stream = [this](bool require_idle) -> cudaStream_t {
        if (streams_.empty()) return nullptr;
        const int count = static_cast<int>(streams_.size());
        const int start = stream_rr_ % std::max(1, count);
        cudaStream_t fallback = nullptr;
        for (int offset = 0; offset < count; ++offset) {
            int idx = (start + offset) % count;
            cudaStream_t cand = streams_[static_cast<size_t>(idx)];
            if (!cand) continue;
            if (!fallback) fallback = cand;
            cudaError_t q = cudaStreamQuery(cand);
            if (q == cudaSuccess) {
                stream_rr_ = (idx + 1) % count;
                return cand;
            }
            if (q != cudaErrorNotReady) {
                cudaGetLastError();
            }
        }
        if (require_idle) return nullptr;
        stream_rr_ = (start + 1) % count;
        return fallback;
    };

    while (running_) {
        cudaStream_t selected_stream = pick_stream(true);
        bool has_idle_stream = (selected_stream != nullptr);

        auto ready = SlotQueue::getInstance().pop_bulk(1, max_wait_ms_);
        if (ready.empty()) {
            if (has_idle_stream) {
                SlotQueue::getInstance().flushPending();
            } else {
                SlotQueue::getInstance().flushPendingIfStale();
            }
            ready = SlotQueue::getInstance().pop_bulk_nowait(1);
        }
        if (ready.empty()) {
            PipelineStats::getInstance().worker_pop_empty.fetch_add(1, std::memory_order_relaxed);
            continue;
        }
        Slot* slot = ready.front().slot;
        if (!slot) continue;

        if (!selected_stream) {
            auto idle_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(1);
            while (running_ && std::chrono::steady_clock::now() < idle_deadline) {
                selected_stream = pick_stream(true);
                if (selected_stream) break;
                std::this_thread::sleep_for(std::chrono::microseconds(200));
            }
        }
        if (!selected_stream) {
            selected_stream = pick_stream(false);
        }

        auto batchRange = nvtxutil::ScopedRange(
            nvtxutil::makeWorkerLabel("WorkerBatch", id_, slot->getCurBatchSize()),
            nvtxutil::color::Control);
        PipelineStats::getInstance().worker_batches_popped.fetch_add(1, std::memory_order_relaxed);
        PipelineStats::getInstance().worker_frames_popped.fetch_add(
            static_cast<uint64_t>(std::max(0, slot->getCurBatchSize())), std::memory_order_relaxed);

        // 使用 worker 持有的 stream 提交异步推理（复用）
        cudaStream_t stream = selected_stream;
        if (!stream) {
            MemoryManager::getInstance().release(slot);
            continue;
        }

        cudaEvent_t ev_pre_start = nullptr;
        cudaEvent_t ev_pre_end = nullptr;
        cudaEvent_t ev_inf_start = nullptr;
        cudaEvent_t ev_inf_end = nullptr;
        bool gpu_timing_enabled = false;
        auto destroy_timing_events = [&](cudaEvent_t& a, cudaEvent_t& b, cudaEvent_t& c, cudaEvent_t& d) {
            if (a) { cudaEventDestroy(a); a = nullptr; }
            if (b) { cudaEventDestroy(b); b = nullptr; }
            if (c) { cudaEventDestroy(c); c = nullptr; }
            if (d) { cudaEventDestroy(d); d = nullptr; }
        };
        if (cudaEventCreate(&ev_pre_start) == cudaSuccess &&
            cudaEventCreate(&ev_pre_end) == cudaSuccess &&
            cudaEventCreate(&ev_inf_start) == cudaSuccess &&
            cudaEventCreate(&ev_inf_end) == cudaSuccess) {
            cudaEventRecord(ev_pre_start, stream);
            gpu_timing_enabled = true;
        } else {
            destroy_timing_events(ev_pre_start, ev_pre_end, ev_inf_start, ev_inf_end);
        }
        auto t_preproc_start = std::chrono::steady_clock::now();
        int model_w = TRTDetector::getInstance().getInputW();
        int model_h = TRTDetector::getInstance().getInputH();
        size_t per_sample_bytes = static_cast<size_t>(3) * model_w * model_h * sizeof(float);
        int batch_size = std::min(slot->getCurBatchSize(), TRTDetector::getInstance().getMaxBatch());
        if (batch_size <= 0) {
            MemoryManager::getInstance().release(slot);
            destroy_timing_events(ev_pre_start, ev_pre_end, ev_inf_start, ev_inf_end);
            continue;
        }

        {
            auto preprocessRange = nvtxutil::ScopedRange(
                nvtxutil::makeWorkerLabel("Preprocess", id_, batch_size),
                nvtxutil::color::Preprocess);
            cudaStreamWaitEvent(stream, slot->getEvent(), 0);
            for (int i = 0; i < batch_size; ++i) {
                float* dst_ptr = reinterpret_cast<float*>(reinterpret_cast<uint8_t*>(slot->getDeviceIn()) + static_cast<size_t>(i) * per_sample_bytes);
                launchNV12ToFloatNCHWDevice(
                    slot->getDeviceNV12Y(i),
                    slot->getDeviceNV12UV(i),
                    slot->getNV12Pitch(),
                    slot->getNV12Pitch(),
                    slot->getNV12W(),
                    slot->getNV12H(),
                    dst_ptr,
                    model_w,
                    model_h,
                    stream);
            }
        }

        // 预处理计时结束
        auto t_preproc_end = std::chrono::steady_clock::now();
        PipelineStats::getInstance().worker_preproc_us.fetch_add(
            static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(t_preproc_end - t_preproc_start).count()),
            std::memory_order_relaxed);

        if (gpu_timing_enabled) {
            cudaEventRecord(ev_pre_end, stream);
            cudaEventRecord(ev_inf_start, stream);
        }

        bool ok = false;
        {
            auto inferRange = nvtxutil::ScopedRange(
                nvtxutil::makeWorkerLabel("Inference", id_, batch_size),
                nvtxutil::color::Inference);
            // 设置 Slot 批次信息
            PipelineStats::getInstance().batches_inferred.fetch_add(1, std::memory_order_relaxed);
            PipelineStats::getInstance().frames_inferred.fetch_add(static_cast<uint64_t>(batch_size), std::memory_order_relaxed);
            slot->setupBatch(batch_size, stream);

            // 提交异步推理，推理完成回调负责释放帧与回收 slot
            ok = TRTDetector::getInstance().asyncInfer(slot, stream,
            [ev_pre_start, ev_pre_end, ev_inf_start, ev_inf_end, gpu_timing_enabled, workerId = id_](Slot* s, bool success) {
                auto postRange = nvtxutil::ScopedRange(
                    nvtxutil::makeWorkerLabel("Postprocess", workerId, s ? s->getCurBatchSize() : 0),
                    nvtxutil::color::Postprocess);
                if (gpu_timing_enabled) {
                    float pre_ms = 0.0f;
                    float inf_ms = 0.0f;
                    if (cudaEventElapsedTime(&pre_ms, ev_pre_start, ev_pre_end) == cudaSuccess) {
                        PipelineStats::getInstance().worker_gpu_preproc_us.fetch_add(
                            static_cast<uint64_t>(pre_ms * 1000.0f), std::memory_order_relaxed);
                    }
                    if (cudaEventElapsedTime(&inf_ms, ev_inf_start, ev_inf_end) == cudaSuccess) {
                        PipelineStats::getInstance().worker_gpu_infer_us.fetch_add(
                            static_cast<uint64_t>(inf_ms * 1000.0f), std::memory_order_relaxed);
                    }
                    PipelineStats::getInstance().worker_gpu_batches.fetch_add(1, std::memory_order_relaxed);
                    cudaEventDestroy(ev_pre_start);
                    cudaEventDestroy(ev_pre_end);
                    cudaEventDestroy(ev_inf_start);
                    cudaEventDestroy(ev_inf_end);
                }
                if (success) {
                    // 后处理：解析推理结果并将检测框坐标缩放回原图
                    auto detections_batch = TRTDetector::getInstance().parseDetections(s);
                    for (int i = 0; i < s->getCurBatchSize(); ++i) {
                        auto meta = s->getPreprocMeta(i);
                        std::vector<DetectionResults::DetectionBox> mapped;
                        mapped.reserve(detections_batch[i].size());
                        for (auto& det : detections_batch[i]) {
                            // 缩放bbox回原图坐标
                            float x = (det.x - meta.pad_w) / meta.scale;
                            float y = (det.y - meta.pad_h) / meta.scale;
                            float w = det.w / meta.scale;
                            float h = det.h / meta.scale;

                            // clamp to image bounds
                            float x0 = std::max(0.0f, std::min(x, static_cast<float>(meta.orig_w - 1)));
                            float y0 = std::max(0.0f, std::min(y, static_cast<float>(meta.orig_h - 1)));
                            float x1 = std::max(0.0f, std::min(x + w, static_cast<float>(meta.orig_w - 1)));
                            float y1 = std::max(0.0f, std::min(y + h, static_cast<float>(meta.orig_h - 1)));

                            DetectionResults::DetectionBox out;
                            out.x = x0;
                            out.y = y0;
                            out.w = std::max(0.0f, x1 - x0);
                            out.h = std::max(0.0f, y1 - y0);
                            out.conf = det.conf;
                            out.class_id = det.class_id;
                            mapped.push_back(out);
                        }
                        int channel_id = s->getSampleChannelId(i);
                        uint64_t epoch = s->getSampleEpoch(i);
                        // 使用 epoch 保护：仅当通道 epoch 匹配时才写入结果
                        // 防止旧视频源的异步回调覆盖新图片源的检测结果
                        if (epoch > 0) {
                            DetectionResults::getInstance().updateIfCurrent(channel_id, epoch, std::move(mapped));
                        } else {
                            DetectionResults::getInstance().update(channel_id, std::move(mapped));
                        }
                        PipelineStats::getInstance().detections_total.fetch_add(
                            static_cast<uint64_t>(detections_batch[i].size()), std::memory_order_relaxed);
                    }
                }
                // 回收 Slot
                MemoryManager::getInstance().release(s);
            },
            gpu_timing_enabled ? ev_inf_end : nullptr
        );
        }

        if (!ok) {
            destroy_timing_events(ev_pre_start, ev_pre_end, ev_inf_start, ev_inf_end);
            MemoryManager::getInstance().release(slot);
        }

        // 立即返回循环，继续拉取下一批（推理为异步进行）
    }
}
