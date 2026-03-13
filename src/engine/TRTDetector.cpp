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
            auto detections_batch = parseDetections(task.host_out, task.batch_size);
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
    std::cout << "[TRTDetector] Ready. Contexts are managed by workers." << std::endl;
    return true;
}

nvinfer1::IExecutionContext* TRTDetector::createContext() {
    if (!engine_) return nullptr;
    auto ctx = engine_->createExecutionContext();
    if (ctx) ctx_created_.fetch_add(1, std::memory_order_relaxed);
    return ctx;
}
bool TRTDetector::asyncInfer(Slot* slot, nvinfer1::IExecutionContext* context,
                             cudaStream_t stream, std::function<void(Slot*, bool)> cb,
                             cudaEvent_t infer_end_event) {
    if (!slot || !context) {
        return false;
    }

    bool launched = inference(slot, context, stream);
    if (!launched) {
        return false;
    }

    if (infer_end_event) {
        cudaEventRecord(infer_end_event, stream);
    }

    int batch_size = slot->getCurBatchSize();
    size_t output_bytes = output_bytes_per_batch_ * static_cast<size_t>(batch_size);
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
            if (parsed_box_size <= 6) {
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
                    if (v4 >= v5) {
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

            if (conf > conf_threshold) {
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
    }
    return results;
}
