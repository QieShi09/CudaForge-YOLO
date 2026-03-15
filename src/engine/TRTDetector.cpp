#include "TRTDetector.hpp"
#include "Logger.hpp"
#include "../core/PipelineStats.hpp"
#include "../core/DetectionResults.hpp"
#include "../core/ChannelResultQueue.hpp"
#include "../core/NvtxUtils.hpp"
#include <fstream>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cmath>
#include <cuda_runtime.h>

namespace {
float iou_xyxy(float ax0, float ay0, float ax1, float ay1,
               float bx0, float by0, float bx1, float by1) {
    const float ix0 = std::max(ax0, bx0);
    const float iy0 = std::max(ay0, by0);
    const float ix1 = std::min(ax1, bx1);
    const float iy1 = std::min(ay1, by1);
    const float iw = std::max(0.0f, ix1 - ix0);
    const float ih = std::max(0.0f, iy1 - iy0);
    const float inter = iw * ih;
    const float a = std::max(0.0f, ax1 - ax0) * std::max(0.0f, ay1 - ay0);
    const float b = std::max(0.0f, bx1 - bx0) * std::max(0.0f, by1 - by0);
    const float uni = a + b - inter;
    if (uni <= 1e-6f) return 0.0f;
    return inter / uni;
}

struct NmsBox {
    TRTDetector::Detection det;
    float x0 = 0.0f;
    float y0 = 0.0f;
    float x1 = 0.0f;
    float y1 = 0.0f;
};
}

TRTDetector::~TRTDetector() {
    shutdown();
}

float* TRTDetector::acquireHostOutputBuffer() {
    {
        std::lock_guard<std::mutex> lk(host_buf_mutex_);
        if (!host_buf_pool_.empty()) {
            float* ptr = host_buf_pool_.back();
            host_buf_pool_.pop_back();
            return ptr;
        }
    }

    void* raw = nullptr;
    cudaError_t err = cudaHostAlloc(&raw, output_size_bytes_, cudaHostAllocDefault);
    if (err != cudaSuccess) return nullptr;
    return static_cast<float*>(raw);
}

void TRTDetector::releaseHostOutputBuffer(float* ptr) {
    if (!ptr) return;
    std::lock_guard<std::mutex> lk(host_buf_mutex_);
    host_buf_pool_.push_back(ptr);
}

void TRTDetector::ensureCallbackWorker() {
    std::lock_guard<std::mutex> lk(callback_mutex_);
    if (callback_worker_running_) return;
    callback_worker_running_ = true;
    callback_worker_ = std::thread(&TRTDetector::callbackWorkerLoop, this);
}

void TRTDetector::stopCallbackWorker() {
    {
        std::lock_guard<std::mutex> lk(callback_mutex_);
        if (!callback_worker_running_) return;
        callback_worker_running_ = false;
    }
    callback_cv_.notify_all();
    if (callback_worker_.joinable()) callback_worker_.join();

    std::lock_guard<std::mutex> lk(callback_mutex_);
    callback_queue_.clear();
}

void TRTDetector::callbackWorkerLoop() {
    while (true) {
        CallbackTask task;
        {
            std::unique_lock<std::mutex> lk(callback_mutex_);
            callback_cv_.wait(lk, [this] {
                return !callback_worker_running_ || !callback_queue_.empty();
            });
            if (!callback_worker_running_ && callback_queue_.empty()) {
                break;
            }
            task = std::move(callback_queue_.front());
            callback_queue_.pop_front();
        }

        bool is_shutting_down = shutting_down_.load(std::memory_order_acquire);
        bool success = !is_shutting_down;

        if (success && task.host_out != nullptr && task.batch_size > 0) {
            auto detections_batch = parseDetections(task.host_out, task.batch_size, getConfidenceThreshold());
            size_t sample_count = std::min(task.samples.size(), detections_batch.size());
            for (size_t i = 0; i < sample_count; ++i) {
                const auto& sm = task.samples[i];
                const auto& dets = detections_batch[i];
                std::vector<DetectionResults::DetectionBox> mapped;
                mapped.reserve(dets.size());
                for (const auto& det : dets) {
                    float scale = (sm.preproc.scale > 1e-6f) ? sm.preproc.scale : 1.0f;

                    // 兼容两类输出：xywh 与 xyxy（后者在这里转换为 xywh）
                    float box_x = det.x;
                    float box_y = det.y;
                    float box_w = det.w;
                    float box_h = det.h;
                    if (det.w > det.x && det.h > det.y) {
                        box_w = det.w - det.x;
                        box_h = det.h - det.y;
                    }

                    float x = (box_x - sm.preproc.pad_w) / scale;
                    float y = (box_y - sm.preproc.pad_h) / scale;
                    float w = box_w / scale;
                    float h = box_h / scale;

                    float x0 = std::max(0.0f, std::min(x, static_cast<float>(sm.preproc.orig_w - 1)));
                    float y0 = std::max(0.0f, std::min(y, static_cast<float>(sm.preproc.orig_h - 1)));
                    float x1 = std::max(0.0f, std::min(x + w, static_cast<float>(sm.preproc.orig_w - 1)));
                    float y1 = std::max(0.0f, std::min(y + h, static_cast<float>(sm.preproc.orig_h - 1)));

                    DetectionResults::DetectionBox out;
                    out.x = x0;
                    out.y = y0;
                    out.w = std::max(0.0f, x1 - x0);
                    out.h = std::max(0.0f, y1 - y0);
                    out.conf = det.conf;
                    out.class_id = det.class_id;
                    mapped.push_back(out);
                }

                ChannelResultQueue::Item item;
                item.epoch = sm.epoch;
                item.detections = std::move(mapped);
                ChannelResultQueue::getInstance().push(sm.channel_id, std::move(item));
                PipelineStats::getInstance().detections_total.fetch_add(
                    static_cast<uint64_t>(dets.size()), std::memory_order_relaxed);
            }
        }

        releaseHostOutputBuffer(task.host_out);

        if (task.cb) {
            task.cb(task.slot, success);
        }

        int prev = inflight_callbacks_.fetch_sub(1, std::memory_order_acq_rel);
        if (prev <= 1) {
            inflight_cv_.notify_all();
        }
    }
}

void TRTDetector::shutdown() {
    if (shutting_down_.exchange(true)) return;

    // 等待所有 in-flight 异步回调完成，防止 use-after-free
    {
        std::unique_lock<std::mutex> lk(inflight_mutex_);
        if (inflight_callbacks_.load(std::memory_order_acquire) > 0) {
            fprintf(stderr, "[TRTDetector] Waiting for %d in-flight callbacks...\n",
                    inflight_callbacks_.load());
            inflight_cv_.wait_for(lk, std::chrono::seconds(5), [this]{
                return inflight_callbacks_.load(std::memory_order_acquire) <= 0;
            });
            if (inflight_callbacks_.load() > 0) {
                fprintf(stderr, "[TRTDetector] WARNING: %d callbacks still in-flight after timeout!\n",
                        inflight_callbacks_.load());
            }
        }
    }

    stopCallbackWorker();

    {
        std::lock_guard<std::mutex> lk(host_buf_mutex_);
        for (float* ptr : host_buf_pool_) {
            if (ptr) cudaFreeHost(ptr);
        }
        host_buf_pool_.clear();
    }

    // 然后释放 engine/runtime
    engine_.reset();
    runtime_.reset();
    {
        std::lock_guard<std::mutex> lk(ctx_mem_mutex_);
        ctx_mem_bytes_.clear();
    }
    ctx_alive_.store(0, std::memory_order_relaxed);
    ctx_active_bytes_.store(0, std::memory_order_relaxed);
    trt_runtime_bytes_.store(0, std::memory_order_relaxed);
    std::cout << "[TRTDetector] Context created: " << ctx_created_.load() << std::endl;
}

bool TRTDetector::load(const std::string& model_path) {
    shutting_down_.store(false, std::memory_order_release);
    ensureCallbackWorker();

    int batch_limit = 32;
    if (const char* env_batch = std::getenv("CUDAFORGE_MAX_DETECT_BATCH")) {
        int parsed = std::atoi(env_batch);
        if (parsed > 0) batch_limit = std::clamp(parsed, 1, 256);
    }

    int dev_count = 0;
    if (cudaGetDeviceCount(&dev_count) != cudaSuccess || dev_count <= 0) {
        std::cerr << "[TRTDetector] No CUDA device available." << std::endl;
        return false;
    }
    cudaSetDevice(0);
    cudaFree(0); // 初始化 CUDA 上下文

    size_t free_before_runtime = 0;
    size_t total_runtime = 0;
    cudaMemGetInfo(&free_before_runtime, &total_runtime);

    std::ifstream file(model_path, std::ios::binary);
    if (!file.good()) {
        std::cerr << "Read model file failed: " << model_path << std::endl;
        return false;
    }

    // 1. 读取模型文件到内存
    file.seekg(0, file.end);
    size_t size = file.tellg();
    file.seekg(0, file.beg);
    std::vector<char> model_data(size);
    file.read(model_data.data(), size);

    // 2. 初始化推理运行时并反序列化 Engine
    runtime_.reset(nvinfer1::createInferRuntime(gLogger));
    if (!runtime_) return false;

    engine_.reset(runtime_->deserializeCudaEngine(model_data.data(), size));
    if (!engine_) return false;

    cudaDeviceSynchronize();
    size_t free_after_runtime = 0;
    size_t total_runtime_after = 0;
    cudaMemGetInfo(&free_after_runtime, &total_runtime_after);
    if (total_runtime > 0 && total_runtime == total_runtime_after && free_before_runtime > free_after_runtime) {
        trt_runtime_bytes_.store(free_before_runtime - free_after_runtime, std::memory_order_relaxed);
    } else {
        trt_runtime_bytes_.store(0, std::memory_order_relaxed);
    }

    // 3. 提取张量元数据 (假设模型是单输入单输出)
    // 注意：V3 API 使用索引 0 和 1 获取 IOTensorName
    input_tensor_name_  = engine_->getIOTensorName(0);
    output_tensor_name_ = engine_->getIOTensorName(1);

    auto in_dims = engine_->getTensorShape(input_tensor_name_.c_str());
    input_h_ = in_dims.d[2];
    input_w_ = in_dims.d[3];

    // 计算输出缓冲区大小（示例：假设输出是 float 类型）
    auto out_dims = engine_->getTensorShape(output_tensor_name_.c_str());
    size_t elements = 1;
    int max_batch = 1; // 默认
    if (in_dims.d[0] == -1) {
        // 动态 batch：从 optimization profile 查询引擎支持的真实最大 batch
        auto maxDims = engine_->getProfileShape(input_tensor_name_.c_str(), 0,
                                                 nvinfer1::OptProfileSelector::kMAX);
        max_batch = maxDims.d[0];
        if (max_batch <= 0) max_batch = 1; // 安全兜底
        std::cout << "[TRTDetector] Dynamic batch detected, profile max batch = " << max_batch << std::endl;
    } else {
        max_batch = in_dims.d[0];
    }
    if (max_batch > batch_limit) {
        std::cout << "[TRTDetector] Clamp max batch: " << max_batch
                  << " -> " << batch_limit << " (CUDAFORGE_MAX_DETECT_BATCH)" << std::endl;
        max_batch = batch_limit;
    }
    for (int i = 0; i < out_dims.nbDims; ++i) {
        if (i == 0 && out_dims.d[0] == -1) {
            elements *= max_batch;
        } else {
            elements *= out_dims.d[i];
        }
    }
    output_size_bytes_ = elements * sizeof(float);
    max_batch_ = max_batch;

    {
        std::lock_guard<std::mutex> lk(host_buf_mutex_);
        for (float* ptr : host_buf_pool_) {
            if (ptr) cudaFreeHost(ptr);
        }
        host_buf_pool_.clear();
    }

    // 计算每个 batch 的输出大小（将动态 batch 视为 1）
    size_t per_batch_elements = 1;
    for (int i = 0; i < out_dims.nbDims; ++i) {
        if (i == 0 && out_dims.d[0] == -1) {
            per_batch_elements *= 1;
        } else {
            per_batch_elements *= out_dims.d[i];
        }
    }
    output_bytes_per_batch_ = per_batch_elements * sizeof(float);
    if (out_dims.nbDims >= 3) {
        output_boxes_ = out_dims.d[1];
        output_box_size_ = out_dims.d[2];
    }

    std::cout << "[TRTDetector] Model Loaded: " << model_path << std::endl;
    std::cout << "[TRTDetector] Input Tensor: " << input_tensor_name_ << ", Shape: [";
    for (int i = 0; i < in_dims.nbDims; ++i) {
        std::cout << in_dims.d[i];
        if (i < in_dims.nbDims - 1) std::cout << ",";
    }
    std::cout << "]" << std::endl;
    std::cout << "[TRTDetector] Output Tensor: " << output_tensor_name_ << ", Shape: [";
    for (int i = 0; i < out_dims.nbDims; ++i) {
        std::cout << out_dims.d[i];
        if (i < out_dims.nbDims - 1) std::cout << ",";
    }
    std::cout << "], Elements: " << elements << ", Size: " << output_size_bytes_ << " bytes" << std::endl;

    // 检测是否支持动态 shape：若 input dims 中存在 <= 0 的维度，视为支持动态
    dynamic_shape_supported_ = false;
    try {
        auto check_dims = engine_->getTensorShape(input_tensor_name_.c_str());
        for (int i = 0; i < check_dims.nbDims; ++i) {
            if (check_dims.d[i] <= 0) { dynamic_shape_supported_ = true; break; }
        }
    } catch (...) {
        dynamic_shape_supported_ = true; // 保守假设
    }

    PipelineStats::getInstance().ctx_pool_size.store(0, std::memory_order_relaxed);
    PipelineStats::getInstance().ctx_pool_hits.store(0, std::memory_order_relaxed);
    PipelineStats::getInstance().ctx_pool_misses.store(0, std::memory_order_relaxed);
    ctx_alive_.store(0, std::memory_order_relaxed);
    ctx_active_bytes_.store(0, std::memory_order_relaxed);
    {
        std::lock_guard<std::mutex> lk(ctx_mem_mutex_);
        ctx_mem_bytes_.clear();
    }
    std::cout << "[TRTDetector] Ready. Contexts are managed by workers." << std::endl;
    return true;
}

nvinfer1::IExecutionContext* TRTDetector::createContext() {
    if (!engine_) return nullptr;
    size_t free_before = 0;
    size_t total_before = 0;
    cudaMemGetInfo(&free_before, &total_before);
    auto ctx = engine_->createExecutionContext();
    if (ctx) {
        cudaDeviceSynchronize();
        size_t free_after = 0;
        size_t total_after = 0;
        cudaMemGetInfo(&free_after, &total_after);
        size_t ctx_bytes = 0;
        if (total_before > 0 && total_before == total_after && free_before > free_after) {
            ctx_bytes = free_before - free_after;
        }
        {
            std::lock_guard<std::mutex> lk(ctx_mem_mutex_);
            ctx_mem_bytes_[ctx] = ctx_bytes;
        }
        ctx_created_.fetch_add(1, std::memory_order_relaxed);
        ctx_alive_.fetch_add(1, std::memory_order_relaxed);
        if (ctx_bytes > 0) {
            ctx_active_bytes_.fetch_add(ctx_bytes, std::memory_order_relaxed);
        }
    }
    return ctx;
}

void TRTDetector::destroyContext(nvinfer1::IExecutionContext* context) {
    if (!context) {
        return;
    }
    size_t ctx_bytes = 0;
    {
        std::lock_guard<std::mutex> lk(ctx_mem_mutex_);
        auto it = ctx_mem_bytes_.find(context);
        if (it != ctx_mem_bytes_.end()) {
            ctx_bytes = it->second;
            ctx_mem_bytes_.erase(it);
        }
    }
    delete context;
    ctx_alive_.fetch_sub(1, std::memory_order_relaxed);
    if (ctx_bytes > 0) {
        size_t cur = ctx_active_bytes_.load(std::memory_order_relaxed);
        while (cur > 0 && !ctx_active_bytes_.compare_exchange_weak(cur, (cur >= ctx_bytes) ? (cur - ctx_bytes) : 0,
                                                                     std::memory_order_relaxed,
                                                                     std::memory_order_relaxed)) {
        }
    }
}
bool TRTDetector::asyncInfer(Slot* slot, nvinfer1::IExecutionContext* context,
                             cudaStream_t stream, std::function<void(Slot*, bool)> cb,
                             cudaEvent_t infer_end_event) {
    if (!slot || !context) {
        return false;
    }

    int batch_size = slot->getCurBatchSize();
    size_t output_bytes = slot->getOutputBytes();
    if (output_bytes == 0) {
        output_bytes = output_bytes_per_batch_ * static_cast<size_t>(batch_size);
    }
    if (output_bytes == 0 || output_bytes > output_size_bytes_) {
        return false;
    }

    float* host_out = acquireHostOutputBuffer();
    if (!host_out) {
        return false;
    }

    void* device_out = slot->getDeviceOut();
    if (!device_out) {
        releaseHostOutputBuffer(host_out);
        return false;
    }

    bool launched = inference(slot, context, stream);
    if (!launched) {
        releaseHostOutputBuffer(host_out);
        return false;
    }

    if (infer_end_event) {
        cudaEventRecord(infer_end_event, stream);
    }

    std::vector<SampleMeta> sample_snapshot;
    sample_snapshot.reserve(static_cast<size_t>(batch_size));
    for (int i = 0; i < batch_size; ++i) {
        SampleMeta sm;
        sm.preproc = slot->getPreprocMeta(i);
        sm.channel_id = slot->getSampleChannelId(i);
        sm.epoch = slot->getSampleEpoch(i);
        sample_snapshot.push_back(sm);
    }

    cudaError_t d2h_err = cudaMemcpyAsync(host_out, device_out, output_bytes,
                                          cudaMemcpyDeviceToHost, stream);
    if (d2h_err != cudaSuccess) {
        // 推理已提交，失败时需要等待 stream 结束，避免调用方立即回收仍在使用的显存
        cudaStreamSynchronize(stream);
        releaseHostOutputBuffer(host_out);
        return false;
    }

    // 当 stream 中的所有任务完成（包括推理），通过 cudaLaunchHostFunc 仅做轻量入队
    CallbackTask* c = new CallbackTask();
    c->slot = slot;
    c->cb = std::move(cb);
    c->host_out = host_out;
    c->output_bytes = output_bytes;
    c->batch_size = batch_size;
    c->samples = std::move(sample_snapshot);

    // 递增 in-flight 计数器，确保 shutdown 前所有回调完成
    inflight_callbacks_.fetch_add(1, std::memory_order_acq_rel);

    cudaError_t err = cudaLaunchHostFunc(stream, [](void* userData){
        CallbackTask* task = static_cast<CallbackTask*>(userData);
        TRTDetector& det = TRTDetector::getInstance();
        {
            std::lock_guard<std::mutex> lk(det.callback_mutex_);
            det.callback_queue_.push_back(std::move(*task));
        }
        det.callback_cv_.notify_one();
        delete task;
    }, c);

    if (err != cudaSuccess) {
        // 回调注册失败，回收 context
        // d2h 已入队，先等待 stream 完成，再安全回收 host_out 并返回失败
        cudaStreamSynchronize(stream);
        inflight_callbacks_.fetch_sub(1, std::memory_order_acq_rel);
        inflight_cv_.notify_all();
        releaseHostOutputBuffer(host_out);
        delete c;
        // 不调用 cb：由调用方 (Worker !ok 分支) 负责释放帧与 Slot
        return false;
    }

    return true;
}

bool TRTDetector::inference(Slot* slot, nvinfer1::IExecutionContext* context, cudaStream_t stream) {
    if (!slot || !context) return false;

    auto inferRange = nvtxutil::ScopedRange("TensorRT::enqueueV3", nvtxutil::color::Inference);

    int batch_size = slot->getCurBatchSize();
    if (batch_size <= 0) return false;

    // 1. 动态设置当前 Batch 的 Shape
    // 如果模型在导出时支持动态 Batch，这一步是必须的
    nvinfer1::Dims4 dims{batch_size, 3, input_h_, input_w_};
    if (!context->setInputShape(input_tensor_name_.c_str(), dims)) {
        return false;
    }

    // 2. 绑定 Slot 提供的显存地址到 Context
    // 这里完全异步，不涉及内存拷贝
    context->setTensorAddress(input_tensor_name_.c_str(), slot->getDeviceIn());
    context->setTensorAddress(output_tensor_name_.c_str(), slot->getDeviceOut());

    // 3. 异步发射推理指令到指定的 Stream
    // 这是顺序排队的第二步。GPU 会确保之前的预处理完成后才开始执行此处指令。
    bool status = context->enqueueV3(stream);

    // 4. 在同一个 Stream 中埋下 Event 锚点
    // 由于 Stream 的 FIFO 特性，当这个 Event 被查询为成功时，
    // 意味着在该流中排在它之前的【预处理】和【推理】任务已全部完成。
    if (status) {
        cudaEventRecord(slot->getEvent(), stream);
    }

    return status;
}

std::vector<std::vector<TRTDetector::Detection>> TRTDetector::parseDetections(const float* host_out,
                                                                              int batch_size,
                                                                              float conf_threshold) {
    std::vector<std::vector<Detection>> results;
    if (!host_out || batch_size <= 0) return results;

    auto postprocessRange = nvtxutil::ScopedRange("TRT::parseDetections", nvtxutil::color::Postprocess);
    PipelineStats::getInstance().postprocess_batches.fetch_add(1, std::memory_order_relaxed);
    PipelineStats::getInstance().postprocess_frames.fetch_add(
        static_cast<uint64_t>(batch_size), std::memory_order_relaxed);

    // 兼容两种常见输出布局：
    // 1) [B, N, C] -> N boxes, C attrs
    // 2) [B, C, N] -> C attrs, N boxes
    int parsed_num_boxes = (output_boxes_ > 0) ? output_boxes_ : 300;
    int parsed_box_size = (output_box_size_ > 0) ? output_box_size_ : 6;

    size_t total_floats = 0;
    if (output_bytes_per_batch_ > 0) {
        total_floats = (output_bytes_per_batch_ / sizeof(float)) * static_cast<size_t>(batch_size);
    } else if (output_size_bytes_ > 0 && max_batch_ > 0) {
        size_t per_batch = (output_size_bytes_ / sizeof(float)) / static_cast<size_t>(max_batch_);
        total_floats = per_batch * static_cast<size_t>(batch_size);
    } else {
        const int num_boxes_fallback = (output_boxes_ > 0) ? output_boxes_ : 300;
        total_floats = static_cast<size_t>(batch_size) * num_boxes_fallback * static_cast<size_t>(parsed_box_size);
    }

    if (total_floats == 0) return results;

    bool transposed_layout = false;
    if (parsed_box_size > 512 && parsed_num_boxes > 0 && parsed_num_boxes <= 256) {
        transposed_layout = true;
        std::swap(parsed_num_boxes, parsed_box_size);
    }

    const size_t per_batch_floats = total_floats / static_cast<size_t>(batch_size);
    if (static_cast<size_t>(parsed_num_boxes) * static_cast<size_t>(parsed_box_size) > per_batch_floats) {
        const int fallback_boxes = static_cast<int>(per_batch_floats / static_cast<size_t>(std::max(6, parsed_box_size)));
        if (fallback_boxes > 0) parsed_num_boxes = fallback_boxes;
    }

    results.resize(batch_size);
    static std::atomic<bool> s_parse_debug_printed{false};
    const bool end2end_nms_free = (!transposed_layout && parsed_num_boxes == 300 && parsed_box_size == 6);
    const float nms_iou_thr = getNmsIouThreshold();
    for (int b = 0; b < batch_size; ++b) {
        const size_t batch_offset = static_cast<size_t>(b) * per_batch_floats;
        for (int i = 0; i < parsed_num_boxes; ++i) {
            float attrs_buf[1024];
            const float* box = nullptr;

            if (!transposed_layout) {
                size_t base = batch_offset + static_cast<size_t>(i) * static_cast<size_t>(parsed_box_size);
                if (base + static_cast<size_t>(parsed_box_size) > total_floats) break;
                box = host_out + base;
            } else {
                if (parsed_box_size > 1024) break;
                for (int c = 0; c < parsed_box_size; ++c) {
                    size_t idx = batch_offset + static_cast<size_t>(c) * static_cast<size_t>(parsed_num_boxes) + static_cast<size_t>(i);
                    if (idx >= total_floats) {
                        attrs_buf[c] = 0.0f;
                    } else {
                        attrs_buf[c] = host_out[idx];
                    }
                }
                box = attrs_buf;
            }

            int class_id = 0;
            float conf = 0.0f;

            // 兼容两类输出：
            // 1) [x,y,w,h,conf,class_id] 或 [x,y,w,h,class_id,conf]
            // 2) [x,y,w,h,obj,cls0,cls1,...]
            if (end2end_nms_free) {
                conf = box[4];
                class_id = static_cast<int>(std::round(box[5]));
            } else if (parsed_box_size <= 6) {
                float v4 = box[4];
                float v5 = box[5];
                auto is_prob = [](float v) {
                    return std::isfinite(v) && v >= 0.0f && v <= 1.0f;
                };
                auto looks_like_cls = [](float v) {
                    return std::isfinite(v) && v >= 0.0f && std::fabs(v - std::round(v)) < 1e-3f && v < 1000.0f;
                };

                bool a_valid = is_prob(v4) && looks_like_cls(v5);
                bool b_valid = is_prob(v5) && looks_like_cls(v4);

                if (a_valid && !b_valid) {
                    conf = v4;
                    class_id = static_cast<int>(std::round(v5));
                } else if (b_valid && !a_valid) {
                    conf = v5;
                    class_id = static_cast<int>(std::round(v4));
                } else if (a_valid && b_valid) {
                    const bool v4_integer_like = looks_like_cls(v4);
                    const bool v5_integer_like = looks_like_cls(v5);
                    if (v5_integer_like && !v4_integer_like) {
                        conf = v4;
                        class_id = static_cast<int>(std::round(v5));
                    } else if (v4_integer_like && !v5_integer_like) {
                        conf = v5;
                        class_id = static_cast<int>(std::round(v4));
                    } else if (v4 >= v5) {
                        conf = v4;
                        class_id = static_cast<int>(std::round(v5));
                    } else {
                        conf = v5;
                        class_id = static_cast<int>(std::round(v4));
                    }
                } else {
                    // 最后兜底：把更像概率的一项当 conf
                    if (std::fabs(v4) <= std::fabs(v5)) {
                        conf = std::clamp(v4, 0.0f, 1.0f);
                        class_id = static_cast<int>(std::max(0.0f, std::round(v5)));
                    } else {
                        conf = std::clamp(v5, 0.0f, 1.0f);
                        class_id = static_cast<int>(std::max(0.0f, std::round(v4)));
                    }
                }
            } else {
                float obj = box[4];
                int best_cls = 0;
                float best_prob = box[5];
                for (int c = 6; c < parsed_box_size; ++c) {
                    if (box[c] > best_prob) {
                        best_prob = box[c];
                        best_cls = c - 5;
                    }
                }
                // 若 obj 不在合理概率区间，退化为直接使用类别概率
                if (obj >= 0.0f && obj <= 1.0f) {
                    conf = obj * best_prob;
                } else {
                    conf = best_prob;
                }
                class_id = best_cls;
            }

            if (std::isfinite(conf) && conf >= conf_threshold && class_id >= 0 &&
                std::isfinite(box[0]) && std::isfinite(box[1]) &&
                std::isfinite(box[2]) && std::isfinite(box[3])) {
                Detection det;
                det.x = box[0];
                det.y = box[1];
                det.w = box[2];
                det.h = box[3];
                det.conf = conf;
                det.class_id = class_id;
                results[b].push_back(det);

                if (!s_parse_debug_printed.load(std::memory_order_relaxed) && b == 0 && i < 3) {
                    bool expected = false;
                    if (s_parse_debug_printed.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
                        std::cerr << "[TRTDetector] Parse debug: transposed=" << (transposed_layout ? 1 : 0)
                                  << " parsed_num_boxes=" << parsed_num_boxes
                                  << " parsed_box_size=" << parsed_box_size
                                  << " raw=[" << box[0] << "," << box[1] << "," << box[2] << "," << box[3]
                                  << "," << box[4] << "," << box[5] << "]"
                                  << " => class=" << class_id << " conf=" << conf << std::endl;
                    }
                }
            }
        }

        auto& dets = results[b];
        if (!end2end_nms_free && !dets.empty()) {
            std::vector<NmsBox> candidates;
            candidates.reserve(dets.size());
            for (const auto& d : dets) {
                NmsBox nb;
                nb.det = d;
                const bool looks_xyxy = (d.w > d.x && d.h > d.y);
                if (looks_xyxy) {
                    nb.x0 = d.x;
                    nb.y0 = d.y;
                    nb.x1 = d.w;
                    nb.y1 = d.h;
                } else {
                    nb.x0 = d.x;
                    nb.y0 = d.y;
                    nb.x1 = d.x + d.w;
                    nb.y1 = d.y + d.h;
                }
                if (nb.x1 <= nb.x0 || nb.y1 <= nb.y0) {
                    continue;
                }
                candidates.push_back(nb);
            }

            std::sort(candidates.begin(), candidates.end(), [](const NmsBox& a, const NmsBox& b) {
                if (a.det.class_id != b.det.class_id) return a.det.class_id < b.det.class_id;
                return a.det.conf > b.det.conf;
            });

            std::vector<Detection> kept;
            kept.reserve(candidates.size());
            for (size_t i = 0; i < candidates.size(); ++i) {
                const auto& cur = candidates[i];
                bool suppressed = false;
                for (const auto& kd : kept) {
                    if (kd.class_id != cur.det.class_id) continue;
                    float kx0, ky0, kx1, ky1;
                    const bool kxyxy = (kd.w > kd.x && kd.h > kd.y);
                    if (kxyxy) {
                        kx0 = kd.x; ky0 = kd.y; kx1 = kd.w; ky1 = kd.h;
                    } else {
                        kx0 = kd.x; ky0 = kd.y; kx1 = kd.x + kd.w; ky1 = kd.y + kd.h;
                    }
                    if (iou_xyxy(cur.x0, cur.y0, cur.x1, cur.y1, kx0, ky0, kx1, ky1) > nms_iou_thr) {
                        suppressed = true;
                        break;
                    }
                }
                if (!suppressed) {
                    kept.push_back(cur.det);
                }
            }
            dets.swap(kept);
        }
    }
    return results;
}
