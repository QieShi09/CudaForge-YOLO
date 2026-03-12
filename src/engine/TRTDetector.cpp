#include "TRTDetector.hpp"
#include "Logger.hpp"
#include "../core/PipelineStats.hpp"
#include "../core/NvtxUtils.hpp"
#include <fstream>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cuda_runtime.h>

TRTDetector::~TRTDetector() {
    shutdown();
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
        releaseContext(task.ctx);
        if (task.cb && !is_shutting_down) {
            task.cb(task.slot, true);
        } else if (task.cb) {
            task.cb(task.slot, false);
        }

        int prev = inflight_callbacks_.fetch_sub(1, std::memory_order_acq_rel);
        if (prev <= 1) {
            inflight_cv_.notify_all();
        }
    }
}

void TRTDetector::shutdown() {
    if (shutting_down_.exchange(true)) return;
    ctx_cv_.notify_all();

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

    // 先释放所有context
    {
        std::lock_guard<std::mutex> lk(ctx_mutex_);
        while (!context_pool_.empty()) {
            auto ctx = context_pool_.front();
            context_pool_.pop_front();
            if (ctx) {
                delete ctx;
                ctx_destroyed_.fetch_add(1, std::memory_order_relaxed);
            }
        }
    }
    // 然后释放 engine/runtime
    engine_.reset();
    runtime_.reset();
    std::cout << "[TRTDetector] Context created: " << ctx_created_.load() << ", destroyed: "
              << ctx_destroyed_.load() << std::endl;
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

    // 并行安全策略：允许多 context（每个 in-flight 推理独占一个 context），
    // 避免“同一 context 并发 enqueue”导致的 binding/shape 状态污染。
    context_pool_limit_ = 32;
    if (const char* env_ctx = std::getenv("CUDAFORGE_MAX_CONTEXTS")) {
        int parsed = std::atoi(env_ctx);
        if (parsed > 0) {
            context_pool_limit_ = static_cast<size_t>(std::clamp(parsed, 1, 64));
        }
    }

    size_t pool_size = 0;
    {
        std::lock_guard<std::mutex> lk(ctx_mutex_);
        while (!context_pool_.empty()) {
            auto* ctx = context_pool_.front();
            context_pool_.pop_front();
            if (ctx) {
                delete ctx;
                ctx_destroyed_.fetch_add(1, std::memory_order_relaxed);
            }
        }
        auto* ctx = createContext();
        if (ctx) {
            context_pool_.push_back(ctx);
        }
        pool_size = context_pool_.size();
    }
    PipelineStats::getInstance().ctx_pool_size.store(static_cast<int>(pool_size), std::memory_order_relaxed);
    std::cout << "[TRTDetector] Context pool size: " << pool_size << std::endl;
    std::cout << "[TRTDetector] Context pool hard limit: " << context_pool_limit_ << std::endl;
    std::cout << "[TRTDetector] Context created: " << ctx_created_.load() << std::endl;
    return pool_size > 0;
}

nvinfer1::IExecutionContext* TRTDetector::createContext() {
    if (!engine_) return nullptr;
    auto ctx = engine_->createExecutionContext();
    if (ctx) ctx_created_.fetch_add(1, std::memory_order_relaxed);
    return ctx;
}

nvinfer1::IExecutionContext* TRTDetector::acquireContext() {
    std::unique_lock<std::mutex> lk(ctx_mutex_);
    bool counted_wait_miss = false;
    while (true) {
        if (shutting_down_.load(std::memory_order_acquire) || !engine_) {
            return nullptr;
        }
        if (!context_pool_.empty()) {
            auto* ctx = context_pool_.front();
            context_pool_.pop_front();
            PipelineStats::getInstance().ctx_pool_hits.fetch_add(1, std::memory_order_relaxed);
            PipelineStats::getInstance().ctx_pool_size.store(static_cast<int>(context_pool_.size()), std::memory_order_relaxed);
            return ctx;
        }
        if (!counted_wait_miss) {
            PipelineStats::getInstance().ctx_pool_misses.fetch_add(1, std::memory_order_relaxed);
            counted_wait_miss = true;
        }
        ctx_cv_.wait_for(lk, std::chrono::milliseconds(1), [this] {
            return shutting_down_.load(std::memory_order_acquire) || !context_pool_.empty() || !engine_;
        });
    }
}

void TRTDetector::releaseContext(nvinfer1::IExecutionContext* ctx) {
    if (!ctx) return;
    std::lock_guard<std::mutex> lk(ctx_mutex_);
    // If engine is already being destroyed, immediately destroy the context
    if (!engine_) {
        delete ctx;
        ctx_destroyed_.fetch_add(1, std::memory_order_relaxed);
    } else {
        context_pool_.push_back(ctx);
        PipelineStats::getInstance().ctx_pool_size.store(static_cast<int>(context_pool_.size()), std::memory_order_relaxed);
    }
    ctx_cv_.notify_one();
}

void TRTDetector::shrinkContextPool(size_t keep) {
    std::lock_guard<std::mutex> lk(ctx_mutex_);
    while (context_pool_.size() > keep) {
        auto* ctx = context_pool_.front();
        context_pool_.pop_front();
        if (ctx) {
            delete ctx;
            ctx_destroyed_.fetch_add(1, std::memory_order_relaxed);
        }
    }
    PipelineStats::getInstance().ctx_pool_size.store(static_cast<int>(context_pool_.size()), std::memory_order_relaxed);
}

void TRTDetector::resizeContextPool(size_t target) {
    if (target == 0) return;
    target = std::min(target, context_pool_limit_);
    std::lock_guard<std::mutex> lk(ctx_mutex_);
    while (context_pool_.size() > target) {
        auto* ctx = context_pool_.front();
        context_pool_.pop_front();
        if (ctx) {
            delete ctx;
            ctx_destroyed_.fetch_add(1, std::memory_order_relaxed);
        }
    }
    while (context_pool_.size() < target) {
        auto* ctx = engine_ ? engine_->createExecutionContext() : nullptr;
        if (!ctx) break;
        ctx_created_.fetch_add(1, std::memory_order_relaxed);
        context_pool_.push_back(ctx);
    }
    PipelineStats::getInstance().ctx_pool_size.store(static_cast<int>(context_pool_.size()), std::memory_order_relaxed);
}

bool TRTDetector::asyncInfer(Slot* slot, cudaStream_t stream, std::function<void(Slot*, bool)> cb,
                             cudaEvent_t infer_end_event) {
    if (!slot) return false;

    nvinfer1::IExecutionContext* ctx = acquireContext();
    if (!ctx) {
        return false;
    }

    bool launched = inference(slot, ctx, stream);
    if (!launched) {
        releaseContext(ctx);
        return false;
    }

    if (infer_end_event) {
        cudaEventRecord(infer_end_event, stream);
    }

    // 当 stream 中的所有任务完成（包括推理），通过 cudaLaunchHostFunc 仅做轻量入队
    CallbackTask* c = new CallbackTask{ctx, slot, cb};

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
        inflight_callbacks_.fetch_sub(1, std::memory_order_acq_rel);
        inflight_cv_.notify_all();
        releaseContext(ctx);
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

std::vector<std::vector<TRTDetector::Detection>> TRTDetector::parseDetections(Slot* slot, float conf_threshold) {
    std::vector<std::vector<Detection>> results;
    if (!slot) return results;

    auto postprocessRange = nvtxutil::ScopedRange("TRT::parseDetections", nvtxutil::color::Postprocess);

    // 确保当前线程绑定正确的 CUDA 设备
    cudaSetDevice(0);

    int batch_size = slot->getCurBatchSize();
    if (batch_size <= 0) return results;
    PipelineStats::getInstance().postprocess_batches.fetch_add(1, std::memory_order_relaxed);
    PipelineStats::getInstance().postprocess_frames.fetch_add(
        static_cast<uint64_t>(batch_size), std::memory_order_relaxed);
    float* output_dev = static_cast<float*>(slot->getDeviceOut());
    if (!output_dev) return results;

    // 假设输出形状 [batch, num_boxes, box_size]
    const int box_size = (output_box_size_ > 0) ? output_box_size_ : 6; // x,y,w,h,conf,class

    size_t total_floats = 0;
    if (output_bytes_per_batch_ > 0) {
        total_floats = (output_bytes_per_batch_ / sizeof(float)) * static_cast<size_t>(batch_size);
    } else if (output_size_bytes_ > 0 && max_batch_ > 0) {
        size_t per_batch = (output_size_bytes_ / sizeof(float)) / static_cast<size_t>(max_batch_);
        total_floats = per_batch * static_cast<size_t>(batch_size);
    } else {
        const int num_boxes_fallback = (output_boxes_ > 0) ? output_boxes_ : 300;
        total_floats = static_cast<size_t>(batch_size) * num_boxes_fallback * box_size;
    }

    if (total_floats == 0) return results;

    std::vector<float> host_out(total_floats);
    size_t bytes_to_copy = total_floats * sizeof(float);

    auto t_d2h_start = std::chrono::steady_clock::now();
    cudaError_t err;
    {
        auto d2hRange = nvtxutil::ScopedRange("TRT::D2H", nvtxutil::color::Postprocess);
        err = cudaMemcpy(host_out.data(), output_dev, bytes_to_copy, cudaMemcpyDeviceToHost);
    }
    auto t_d2h_end = std::chrono::steady_clock::now();
    PipelineStats::getInstance().postprocess_d2h_us.fetch_add(
        static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(t_d2h_end - t_d2h_start).count()),
        std::memory_order_relaxed);
    if (err != cudaSuccess) {
        std::cerr << "[TRTDetector] cudaMemcpy output failed: " << cudaGetErrorString(err) << " (code=" << err << ")" << std::endl;
        return results;
    }

    const int max_boxes = static_cast<int>(total_floats / static_cast<size_t>(box_size));
    const int num_boxes = (output_boxes_ > 0) ? std::min(output_boxes_, max_boxes) : max_boxes;

    results.resize(batch_size);
    for (int b = 0; b < batch_size; ++b) {
        for (int i = 0; i < num_boxes; ++i) {
            size_t base = (static_cast<size_t>(b) * num_boxes + i) * box_size;
            if (base + static_cast<size_t>(box_size) > total_floats) break;
            const float* box = host_out.data() + base;
            float conf = box[4];
            if (conf > conf_threshold) {
                Detection det;
                det.x = box[0];
                det.y = box[1];
                det.w = box[2];
                det.h = box[3];
                det.conf = conf;
                det.class_id = static_cast<int>(box[5]);
                results[b].push_back(det);
            }
        }
    }
    return results;
}
