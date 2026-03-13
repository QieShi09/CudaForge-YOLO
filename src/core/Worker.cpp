#include "Worker.hpp"
#include "InputFrameArenaStore.hpp"
#include "SlotPool.hpp"
#include "TensorArenaManager.hpp"
#include "TRTDetector.hpp"
#include "Slot.hpp"
#include "PipelineStats.hpp"
#include "NvtxUtils.hpp"
#include "../engine/TRTWorker.hpp"
#include <cuda_runtime_api.h>
#include "../kernels/CudaImageProc.cuh"
#include <iostream>
#include <cstdlib>
#include <algorithm>

Worker::Worker(int id, size_t max_batch, std::chrono::milliseconds max_wait_ms)
        : id_(id),
            max_batch_(std::max<size_t>(1, max_batch)),
            max_wait_ms_(max_wait_ms) {}

Worker::~Worker() { stop(); }

void Worker::start() {
    if (running_.exchange(true)) return;
    trt_worker_ = std::make_unique<TRTWorker>(id_);
    if (!trt_worker_->init()) {
        trt_worker_.reset();
        running_.store(false, std::memory_order_release);
        std::cerr << "[Worker] TRTWorker init failed, worker=" << id_ << std::endl;
        return;
    }
    thr_ = std::thread(&Worker::run, this);
}

void Worker::stop() {
    if (!running_.exchange(false)) return;
    if (thr_.joinable()) thr_.join();
    if (trt_worker_) {
        trt_worker_->shutdown();
        trt_worker_.reset();
    }
}

void Worker::run() {
    // 确保 worker 线程绑定正确的 CUDA 设备
    cudaSetDevice(0);

    while (running_) {
        int inflight = inflight_infers_.load(std::memory_order_relaxed);
        size_t desired_max_batch = std::max<size_t>(1, max_batch_);
        size_t desired_min_batch = (inflight < max_inflight_per_worker_) ? std::min<size_t>(desired_max_batch, 4) : 1;

        auto samples = InputFrameArenaStore::getInstance().popBatch(
            desired_max_batch,
            desired_min_batch,
            max_wait_ms_);

        if (samples.empty()) {
            PipelineStats::getInstance().worker_pop_empty.fetch_add(1, std::memory_order_relaxed);
            continue;
        }

        Slot* slot = SlotPool::getInstance().pop();
        if (!slot) continue;

        slot->clear();
        for (size_t i = 0; i < samples.size(); ++i) {
            const auto& sm = samples[i];
            slot->appendSampleMeta(sm.channel_id, sm.epoch, sm.preproc);
            slot->setFrameTimestamp(static_cast<int>(i), sm.timestamp_us);
        }

        auto batchRange = nvtxutil::ScopedRange(
            nvtxutil::makeWorkerLabel("WorkerBatch", id_, slot->getCurBatchSize()),
            nvtxutil::color::Control);
        PipelineStats::getInstance().worker_batches_popped.fetch_add(1, std::memory_order_relaxed);
        PipelineStats::getInstance().worker_frames_popped.fetch_add(
            static_cast<uint64_t>(std::max(0, slot->getCurBatchSize())), std::memory_order_relaxed);

        // 使用 worker 持有的 stream 提交异步推理（复用）
        cudaStream_t stream = trt_worker_ ? trt_worker_->stream() : nullptr;
        if (!stream) {
            InputFrameArenaStore::getInstance().releaseBatchNow(samples);
            SlotPool::getInstance().push(slot);
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
        int detector_max_batch = TRTDetector::getInstance().getMaxBatch();
        if (detector_max_batch <= 0) detector_max_batch = 1;
        int batch_size = std::min(static_cast<int>(samples.size()), detector_max_batch);
        if (batch_size <= 0) {
            InputFrameArenaStore::getInstance().releaseBatchNow(samples);
            SlotPool::getInstance().push(slot);
            destroy_timing_events(ev_pre_start, ev_pre_end, ev_inf_start, ev_inf_end);
            continue;
        }

        size_t input_bytes = static_cast<size_t>(batch_size) * per_sample_bytes;
        size_t output_bytes = TRTDetector::getInstance().getOutputBytesPerBatch() * static_cast<size_t>(batch_size);
        void* dev_in = TensorArenaManager::getInstance().allocateInput(input_bytes);
        void* dev_out = TensorArenaManager::getInstance().allocateOutput(output_bytes);
        if (!dev_in || !dev_out) {
            if (dev_in) TensorArenaManager::getInstance().deallocateInput(dev_in, input_bytes);
            if (dev_out) TensorArenaManager::getInstance().deallocateOutput(dev_out, output_bytes);
            InputFrameArenaStore::getInstance().releaseBatchNow(samples);
            SlotPool::getInstance().push(slot);
            destroy_timing_events(ev_pre_start, ev_pre_end, ev_inf_start, ev_inf_end);
            continue;
        }
        slot->setDeviceIn(dev_in, input_bytes);
        slot->setDeviceOut(dev_out, output_bytes);

        {
            auto preprocessRange = nvtxutil::ScopedRange(
                nvtxutil::makeWorkerLabel("Preprocess", id_, batch_size),
                nvtxutil::color::Preprocess);
            for (int i = 0; i < batch_size; ++i) {
                const auto& sm = samples[static_cast<size_t>(i)];
                if (sm.ready_event) {
                    cudaStreamWaitEvent(stream, sm.ready_event, 0);
                }
                uint8_t* src_y = reinterpret_cast<uint8_t*>(sm.ptr);
                uint8_t* src_uv = src_y + static_cast<size_t>(model_w) * model_h;
                float* dst_ptr = reinterpret_cast<float*>(reinterpret_cast<uint8_t*>(dev_in) + static_cast<size_t>(i) * per_sample_bytes);
                launchNV12ToFloatNCHWDevice(
                    src_y,
                    src_uv,
                    model_w,
                    model_w,
                    model_w,
                    model_h,
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
            ok = TRTDetector::getInstance().asyncInfer(slot,
            trt_worker_ ? trt_worker_->context() : nullptr,
            stream,
            [this, ev_pre_start, ev_pre_end, ev_inf_start, ev_inf_end, gpu_timing_enabled, workerId = id_](Slot* s, bool success) {
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
                (void)success;
                if (s && s->getDeviceOut() && s->getOutputBytes() > 0) {
                    TensorArenaManager::getInstance().deallocateOutput(s->getDeviceOut(), s->getOutputBytes());
                }
                inflight_infers_.fetch_sub(1, std::memory_order_relaxed);
                // 回收 Slot
                SlotPool::getInstance().push(s);
            },
            slot->getEvent()
        );
        }

        if (!ok) {
            static std::atomic<uint64_t> s_submit_fail_count{0};
            uint64_t fail_n = s_submit_fail_count.fetch_add(1, std::memory_order_relaxed) + 1;
            if (fail_n % 50 == 1) {
                std::cerr << "[Worker " << id_ << "] asyncInfer submit failed x" << fail_n
                          << " (samples=" << samples.size() << ", detMax=" << TRTDetector::getInstance().getMaxBatch() << ")" << std::endl;
            }
            InputFrameArenaStore::getInstance().releaseBatchNow(samples);
            if (slot->getDeviceIn() && slot->getInputBytes() > 0) {
                TensorArenaManager::getInstance().deallocateInput(slot->getDeviceIn(), slot->getInputBytes());
            }
            if (slot->getDeviceOut() && slot->getOutputBytes() > 0) {
                TensorArenaManager::getInstance().deallocateOutput(slot->getDeviceOut(), slot->getOutputBytes());
            }
            destroy_timing_events(ev_pre_start, ev_pre_end, ev_inf_start, ev_inf_end);
            SlotPool::getInstance().push(slot);
        } else {
            InputFrameArenaStore::getInstance().releaseBatchAfter(
                samples,
                slot->getEvent());
            TensorArenaManager::getInstance().deallocateInputAfter(
                slot->getDeviceIn(), slot->getInputBytes(),
                slot->getEvent());
            inflight_infers_.fetch_add(1, std::memory_order_relaxed);
        }

        // 立即返回循环，继续拉取下一批（推理为异步进行）
    }
}
