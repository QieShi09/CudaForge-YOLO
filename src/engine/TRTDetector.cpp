#include "TRTDetector.hpp"
#include "Logger.hpp"
#include "../core/PipelineStats.hpp"
#include <fstream>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cuda_runtime.h>

TRTDetector::~TRTDetector() {
    shutdown();
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
    int batch_limit = 16;
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

    // 读取 context 池硬上限（默认 2，可通过环境变量覆盖）
    {
        context_pool_limit_ = 2;
        if (const char* env_limit = std::getenv("CUDAFORGE_MAX_CONTEXTS")) {
            int parsed = std::atoi(env_limit);
            if (parsed > 0) context_pool_limit_ = static_cast<size_t>(parsed);
        }
    }

    // 初始化 context 池（按 GPU 可用显存动态创建，每个 context 约占 400-500 MiB）
    // 策略：少 Worker + 大 batch，因此只需 1-2 个 context（1 活跃 + 1 备用）
    size_t pool_size = 0;
    const size_t RESERVED_MiB = 2048; // 为解码器 + Slot + 系统预留 2 GiB
    {
        size_t free_bytes = 0, total_bytes = 0;
        cudaMemGetInfo(&free_bytes, &total_bytes);
        size_t free_mib = free_bytes / (1024 * 1024);
        size_t available_for_ctx = (free_mib > RESERVED_MiB) ? (free_mib - RESERVED_MiB) : 0;
        // 估算每个 context 大小为 500 MiB（保守值）
        size_t max_by_mem = available_for_ctx / 500;
        pool_size = std::max(static_cast<size_t>(1), std::min(max_by_mem, context_pool_limit_));
        std::cout << "[TRTDetector] GPU free: " << free_mib << " MiB, reserved: " 
                  << RESERVED_MiB << " MiB, max contexts by memory: " << max_by_mem << std::endl;
    }
    for (size_t i = 0; i < pool_size; ++i) {
        // 每创建一个 context 后检查剩余显存
        auto ctx = createContext();
        if (ctx) {
            std::lock_guard<std::mutex> lk(ctx_mutex_);
            context_pool_.push_back(ctx);
        }
        size_t free_after = 0, total_after = 0;
        cudaMemGetInfo(&free_after, &total_after);
        size_t free_mib_after = free_after / (1024 * 1024);
        if (free_mib_after < RESERVED_MiB) {
            std::cout << "[TRTDetector] Stopping at " << (i+1) << " contexts (free=" 
                      << free_mib_after << " MiB < reserved=" << RESERVED_MiB << " MiB)" << std::endl;
            break;
        }
    }
    {
        std::lock_guard<std::mutex> lk(ctx_mutex_);
        pool_size = context_pool_.size();
    }
    PipelineStats::getInstance().ctx_pool_size.store(static_cast<int>(pool_size), std::memory_order_relaxed);
    std::cout << "[TRTDetector] Context pool size: " << pool_size << std::endl;
    std::cout << "[TRTDetector] Context pool hard limit: " << context_pool_limit_ << std::endl;
    std::cout << "[TRTDetector] Context created: " << ctx_created_.load() << std::endl;
    return true;
}

nvinfer1::IExecutionContext* TRTDetector::createContext() {
    if (!engine_) return nullptr;
    auto ctx = engine_->createExecutionContext();
    if (ctx) ctx_created_.fetch_add(1, std::memory_order_relaxed);
    return ctx;
}

nvinfer1::IExecutionContext* TRTDetector::acquireContext() {
    std::lock_guard<std::mutex> lk(ctx_mutex_);
    if (!context_pool_.empty()) {
        auto ctx = context_pool_.front();
        context_pool_.pop_front();
        PipelineStats::getInstance().ctx_pool_hits.fetch_add(1, std::memory_order_relaxed);
        PipelineStats::getInstance().ctx_pool_size.store(static_cast<int>(context_pool_.size()), std::memory_order_relaxed);
        return ctx;
    }
    // 池空时尝试创建新的 context
    PipelineStats::getInstance().ctx_pool_misses.fetch_add(1, std::memory_order_relaxed);
    const int created = ctx_created_.load(std::memory_order_relaxed);
    const int destroyed = ctx_destroyed_.load(std::memory_order_relaxed);
    const int alive = std::max(0, created - destroyed);
    if (static_cast<size_t>(alive) >= context_pool_limit_) {
        return nullptr;
    }
    return createContext();
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
}

void TRTDetector::shrinkContextPool(size_t keep) {
    std::lock_guard<std::mutex> lk(ctx_mutex_);
    if (context_pool_.size() <= keep) return;
    while (context_pool_.size() > keep) {
        auto ctx = context_pool_.front();
        context_pool_.pop_front();
        if (ctx) {
            delete ctx;
            ctx_destroyed_.fetch_add(1, std::memory_order_relaxed);
        }
    }
    PipelineStats::getInstance().ctx_pool_size.store(static_cast<int>(context_pool_.size()), std::memory_order_relaxed);
    std::cout << "[TRTDetector] Shrunk context pool to " << context_pool_.size() << std::endl;
}

void TRTDetector::resizeContextPool(size_t target) {
    if (target == 0) return; // 0 = 自动，不调整
    target = std::min(target, context_pool_limit_);
    std::lock_guard<std::mutex> lk(ctx_mutex_);
    // 缩减
    while (context_pool_.size() > target) {
        auto ctx = context_pool_.front();
        context_pool_.pop_front();
        if (ctx) {
            delete ctx;
            ctx_destroyed_.fetch_add(1, std::memory_order_relaxed);
        }
    }
    // 扩充
    while (context_pool_.size() < target) {
        auto ctx = engine_ ? engine_->createExecutionContext() : nullptr;
        if (!ctx) {
            std::cerr << "[TRTDetector] Failed to create context for pool expansion" << std::endl;
            break;
        }
        ctx_created_.fetch_add(1, std::memory_order_relaxed);
        context_pool_.push_back(ctx);
        // 检查显存
        size_t free_bytes = 0, total_bytes = 0;
        cudaMemGetInfo(&free_bytes, &total_bytes);
        size_t free_mib = free_bytes / (1024 * 1024);
        if (free_mib < 1024) { // 剩余不足 1 GiB 时停止扩充
            std::cout << "[TRTDetector] Stopped pool expansion at " << context_pool_.size()
                      << " (free=" << free_mib << " MiB)" << std::endl;
            break;
        }
    }
    PipelineStats::getInstance().ctx_pool_size.store(static_cast<int>(context_pool_.size()), std::memory_order_relaxed);
    std::cout << "[TRTDetector] Resized context pool to " << context_pool_.size()
              << " (target=" << target << ", hard_limit=" << context_pool_limit_ << ")" << std::endl;
}

bool TRTDetector::asyncInfer(Slot* slot, cudaStream_t stream, std::function<void(Slot*, bool)> cb,
                             cudaEvent_t infer_end_event) {
    if (!slot) return false;

    nvinfer1::IExecutionContext* ctx = acquireContext();
    if (!ctx) {
        // 不调用 cb：由调用方 (Worker !ok 分支) 负责释放帧与 Slot
        return false;
    }

    bool launched = inference(slot, ctx, stream);
    if (!launched) {
        releaseContext(ctx);
        // 不调用 cb：由调用方 (Worker !ok 分支) 负责释放帧与 Slot
        return false;
    }

    if (infer_end_event) {
        cudaEventRecord(infer_end_event, stream);
    }

    // 当 stream 中的所有任务完成（包括推理），通过 cudaLaunchHostFunc 回调上层
    struct CbCtx {
        TRTDetector* self;
        nvinfer1::IExecutionContext* ctx;
        Slot* slot;
        std::function<void(Slot*, bool)> cb;
    };

    CbCtx* c = new CbCtx{this, ctx, slot, cb};

    // 递增 in-flight 计数器，确保 shutdown 前所有回调完成
    inflight_callbacks_.fetch_add(1, std::memory_order_acq_rel);

    cudaError_t err = cudaLaunchHostFunc(stream, [](void* userData){
        CbCtx* cc = static_cast<CbCtx*>(userData);
        // 在 host callback 中不要直接调用 CUDA runtime API（禁止），改为在新线程中执行回调。
        std::thread([cc]() {
            // [安全检查] 如果正在 shutdown，跳过后处理，仅做必要的清理
            bool is_shutting_down = cc->self->shutting_down_.load(std::memory_order_acquire);
            cc->self->releaseContext(cc->ctx);
            if (cc->cb && !is_shutting_down) {
                cc->cb(cc->slot, true);
            } else if (cc->cb) {
                // shutdown 中仍需释放帧和 slot，但跳过后处理
                cc->cb(cc->slot, false);
            }
            // 递减 in-flight 计数器并通知 shutdown 等待者
            int prev = cc->self->inflight_callbacks_.fetch_sub(1, std::memory_order_acq_rel);
            if (prev <= 1) {
                cc->self->inflight_cv_.notify_all();
            }
            delete cc;
        }).detach();
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
    cudaError_t err = cudaMemcpy(host_out.data(), output_dev, bytes_to_copy, cudaMemcpyDeviceToHost);
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
