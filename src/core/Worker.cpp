#include "Worker.hpp"
#include "DetectionQueue.hpp"
#include "MemoryManager.hpp"
#include "TRTDetector.hpp"
#include "Slot.hpp"
#include "DetectionResults.hpp"
#include "PipelineStats.hpp"
#include <cuda_runtime_api.h>
#include "../kernels/CudaImageProc.cuh"
#include <libavutil/pixfmt.h>
#include <iostream>
#include <cstdlib>
#include <algorithm>

Worker::Worker(int id, size_t max_batch, std::chrono::milliseconds max_wait_ms, int total_workers)
    : id_(id), max_batch_(std::max<size_t>(1, max_batch)), max_wait_ms_(max_wait_ms), total_workers_(total_workers) {}

Worker::~Worker() { stop(); }

void Worker::start() {
    if (running_.exchange(true)) return;
    cudaSetDevice(0);
    // 创建并持有一个 stream 供该 worker 重复使用（减少创建销毁开销）
    cudaStreamCreateWithFlags(&stream_, cudaStreamNonBlocking);
    // 为零拷贝后仍需要的临时 NV12 缓冲分配一次（按模型输入大小）
    int model_w = TRTDetector::getInstance().getInputW();
    int model_h = TRTDetector::getInstance().getInputH();
    size_t y_bytes = static_cast<size_t>(model_w) * model_h;
    size_t uv_bytes = static_cast<size_t>(model_w) * (model_h / 2);
    tmp_dev_y_bytes_ = y_bytes;
    tmp_dev_uv_bytes_ = uv_bytes;
    if (cudaMalloc(&tmp_dev_y_, y_bytes) != cudaSuccess) {
        fprintf(stderr, "Fatal: cudaMalloc tmp_dev_y_ failed.\n");
        std::terminate();
    }
    if (cudaMalloc(&tmp_dev_uv_, uv_bytes) != cudaSuccess) {
        fprintf(stderr, "Fatal: cudaMalloc tmp_dev_uv_ failed.\n");
        std::terminate();
    }
    thr_ = std::thread(&Worker::run, this);
}

void Worker::stop() {
    if (!running_.exchange(false)) return;
    if (thr_.joinable()) thr_.join();
    // 销毁该 worker 持有的 stream
    if (stream_) {
        cudaStreamDestroy(stream_);
        stream_ = nullptr;
    }
    // 释放 per-worker 临时缓冲
    if (tmp_dev_y_) { cudaFree(tmp_dev_y_); tmp_dev_y_ = nullptr; }
    if (tmp_dev_uv_) { cudaFree(tmp_dev_uv_); tmp_dev_uv_ = nullptr; }
}

void Worker::run() {
    // 确保 worker 线程绑定正确的 CUDA 设备
    cudaSetDevice(0);

    while (running_) {
        // 【自适应 stream 策略】
        // · 单 Worker 模式：sync 等 GPU 推理完再 pop → 帧在 DQ 中积累 → 大 batch → 高吞吐
        // · 多 Worker 模式：跳过 sync，各 worker 独立快速提交到各自 stream
        //   ─ 避免所有 worker 同时 sync→同时醒来→争抢 DQ 导致 batch=1
        //   ─ Slot 池提供自然背压（acquire 阻塞）
        //   ─ 多 stream 并行在 GPU 上重叠执行，提升吞吐
        if (stream_ && total_workers_ <= 1) {
            if (cudaStreamQuery(stream_) == cudaErrorNotReady) {
                cudaStreamSynchronize(stream_);
            }
        }

        // 实际可用 batch 不能超过模型支持的最大 batch，否则输出缓冲区溢出
        size_t effective_batch = std::min(max_batch_,
            static_cast<size_t>(TRTDetector::getInstance().getMaxBatch()));
        effective_batch = std::min(effective_batch, static_cast<size_t>(16));
        // 从全局 DetectionQueue 一次性弹出最多 effective_batch 帧
        auto batch = DetectionQueue::getInstance().pop_bulk(effective_batch, max_wait_ms_);
        if (batch.empty()) {
            PipelineStats::getInstance().worker_pop_empty.fetch_add(1, std::memory_order_relaxed);
            continue;
        }
        PipelineStats::getInstance().worker_batches_popped.fetch_add(1, std::memory_order_relaxed);
        PipelineStats::getInstance().worker_frames_popped.fetch_add(
            static_cast<uint64_t>(batch.size()), std::memory_order_relaxed);

        // 申请 Slot（阻塞直到有可用 Slot）— 记录等待时间
        auto t_slot_start = std::chrono::steady_clock::now();
        Slot* slot = MemoryManager::getInstance().acquire();
        auto t_slot_end = std::chrono::steady_clock::now();
        PipelineStats::getInstance().worker_slot_wait_us.fetch_add(
            static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(t_slot_end - t_slot_start).count()),
            std::memory_order_relaxed);
        if (!slot) {
            // acquire 返回 nullptr 可能是 shutdown，检查 running_ 尽快退出
            if (!running_) {
                for (auto &it : batch) if (it.frame) { AVFrame* fr = it.frame; av_frame_free(&fr); }
                break;
            }
            // 无 slot 时直接释放帧并继续
            for (auto &it : batch) if (it.frame) { AVFrame* fr = it.frame; av_frame_free(&fr); }
            continue;
        }

        // 使用 worker 持有的 stream 提交异步推理（复用）
        cudaStream_t stream = stream_;

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


        // 准备 src 指针与元数据（使用 DetectionQueue Item 中的信息）
        std::vector<void*> src_ptrs;
        std::vector<int> ch_ids;
        std::vector<int64_t> ts;
        std::vector<int> src_formats;
        std::vector<int> detection_steps;
        src_ptrs.reserve(batch.size()); ch_ids.reserve(batch.size()); ts.reserve(batch.size());
        src_formats.reserve(batch.size()); detection_steps.reserve(batch.size());
        for (auto &it : batch) {
            if (!it.frame) continue;
            src_ptrs.push_back(it.frame->data[0]);
            // 使用来自 DetectionQueue 的 channel_id，而不是 worker id
            ch_ids.push_back(it.channel_id);
            ts.push_back(it.frame->pts);
            src_formats.push_back(it.src_format);
            detection_steps.push_back(it.detection_step);
        }

        // --- 预处理：对每个样本执行 NV12->RGBA（如需要）并 resize+letterbox -> float NCHW 写入 slot 输入缓冲 ---
        auto t_preproc_start = std::chrono::steady_clock::now();
        int model_w = TRTDetector::getInstance().getInputW();
        int model_h = TRTDetector::getInstance().getInputH();
        size_t per_sample_bytes = static_cast<size_t>(3) * model_w * model_h * sizeof(float);

        // 为每个样本执行预处理并写入 slot->getDeviceIn() + offset
        for (int i = 0; i < static_cast<int>(src_ptrs.size()); ++i) {
            if (!batch[i].frame) continue;
            AVFrame* fr = batch[i].frame;
            int sfmt = src_formats[i];
            float* dst_ptr = reinterpret_cast<float*>(reinterpret_cast<uint8_t*>(slot->getDeviceIn()) + i * per_sample_bytes);

            int src_w = fr->width;
            int src_h = fr->height;

            // 计算 letterbox scale 和 pad
            float r = std::min(static_cast<float>(model_w) / src_w, static_cast<float>(model_h) / src_h);
            int new_w = static_cast<int>(src_w * r);
            int new_h = static_cast<int>(src_h * r);
            Slot::PreprocMeta meta;
            meta.orig_w = src_w;
            meta.orig_h = src_h;
            meta.scale = r;
            meta.pad_w = (model_w - new_w) / 2;
            meta.pad_h = (model_h - new_h) / 2;
            slot->setPreprocMeta(i, meta);

            if (sfmt == AV_PIX_FMT_NV12) {
                int y_pitch = fr->linesize[0];
                int uv_pitch = fr->linesize[1];

                if (fr->format == AV_PIX_FMT_CUDA) {
                    // 先对 NV12 在 device 上做 resize -> 写入 per-worker 临时 NV12 缓冲，再做色彩转换
                    const uint8_t* dev_y = reinterpret_cast<const uint8_t*>(fr->data[0]);
                    const uint8_t* dev_uv = reinterpret_cast<const uint8_t*>(fr->data[1]);
                    // 目标 NV12 的行跨度（简单使用 dst_w）
                    int dst_y_pitch = model_w;
                    int dst_uv_pitch = model_w;
                    launchResizeNV12ToNV12Device(dev_y, dev_uv, y_pitch, uv_pitch, src_w, src_h,
                                                 tmp_dev_y_, tmp_dev_uv_, dst_y_pitch, dst_uv_pitch,
                                                 model_w, model_h, stream);
                    // 然后把已缩放的 NV12 转成模型输入 float NCHW
                    launchNV12ToFloatNCHWDevice(tmp_dev_y_, tmp_dev_uv_, dst_y_pitch, dst_uv_pitch,
                                                model_w, model_h, dst_ptr, model_w, model_h, stream);
                } else {
                    fprintf(stderr, "Fatal: non-CUDA NV12 frame received (channel %d, pts=%lld). Aborting.\n", ch_ids[i], (long long)ts[i]);
                    std::terminate();
                }
            } else if (sfmt == AV_PIX_FMT_RGBA) {
                if (fr->format != AV_PIX_FMT_CUDA) {
                    fprintf(stderr, "Fatal: non-CUDA RGBA frame received (channel %d, pts=%lld). Aborting.\n", ch_ids[i], (long long)ts[i]);
                    std::terminate();
                }
                const uint8_t* dev_rgba = reinterpret_cast<const uint8_t*>(fr->data[0]);
                int src_pitch = fr->linesize[0];
                launchResizeLetterboxToFloatNCHW(dev_rgba, src_pitch, src_w, src_h,
                                                 dst_ptr, model_w, model_h, stream);
            } else {
                fprintf(stderr, "Fatal: unsupported frame format %d received (channel %d). Aborting.\n", sfmt, ch_ids[i]);
                std::terminate();
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

        // 设置 Slot 批次信息
        PipelineStats::getInstance().batches_inferred.fetch_add(1, std::memory_order_relaxed);
        PipelineStats::getInstance().frames_inferred.fetch_add(static_cast<uint64_t>(src_ptrs.size()), std::memory_order_relaxed);
        slot->setupBatch(ch_ids, ts, src_formats, detection_steps, stream);

        // 提交异步推理，推理完成回调负责释放帧与回收 slot
        bool ok = TRTDetector::getInstance().asyncInfer(slot, stream,
            [batch, ev_pre_start, ev_pre_end, ev_inf_start, ev_inf_end, gpu_timing_enabled](Slot* s, bool success) {
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
                        int channel_id = 0;
                        uint64_t epoch = 0;
                        if (static_cast<size_t>(i) < batch.size()) {
                            channel_id = batch[i].channel_id;
                            epoch = batch[i].epoch;
                        }
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
                // 释放输入帧（在副本上操作）
                for (auto &it : batch) if (it.frame) { AVFrame* fr = it.frame; av_frame_free(&fr); }
                // 回收 Slot
                MemoryManager::getInstance().release(s);
            },
            gpu_timing_enabled ? ev_inf_end : nullptr
        );

        if (!ok) {
            destroy_timing_events(ev_pre_start, ev_pre_end, ev_inf_start, ev_inf_end);
            // 若异步提交失败，主动回收并释放
            MemoryManager::getInstance().release(slot);
            for (auto &it : batch) if (it.frame) { AVFrame* fr = it.frame; av_frame_free(&fr); }
            // 提交失败：释放 slot 并释放帧
        }

        // 立即返回循环，继续拉取下一批（推理为异步进行）
    }
}
