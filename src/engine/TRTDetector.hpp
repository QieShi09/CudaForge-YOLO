#ifndef TRT_DETECTOR_HPP
#define TRT_DETECTOR_HPP

#include <NvInfer.h>
#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include "Slot.hpp"
#include <mutex>
#include <deque>
#include <functional>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <cstddef>

/**
 * @brief TRTDetector (单例/共享资源类)
 * 职责：
 * 1. 加载并持有 TensorRT Engine（只读，多线程安全）。
 * 2. 提供创建私有 ExecutionContext 的工厂接口。
 * 3. 执行核心推理 Enqueue 动作。
 */
class TRTDetector {
public:
    static TRTDetector& getInstance() {
        static TRTDetector instance;
        return instance;
    }

    /**
     * @brief 加载 TensorRT 模型文件 (.engine / .trt)
     * @param model_path 模型文件路径
     * @return 是否加载成功
     */
    bool load(const std::string& model_path);
    void shutdown();

    /**
     * @brief 为 Worker 线程创建私有的执行上下文
     * 由于 IExecutionContext 包含中间激活值缓冲区，非线程安全，
     * 所以每个并发的 Inference Worker 必须通过此接口持有一个私有实例。
     */
    nvinfer1::IExecutionContext* createContext();

    // 池化获取/归还 context
    nvinfer1::IExecutionContext* acquireContext();
    void releaseContext(nvinfer1::IExecutionContext* ctx);

    /**
     * @brief 主动缩减 context 池，销毁多余的 ExecutionContext 来释放显存
     * @param keep 保留的最小 context 数量
     */
    void shrinkContextPool(size_t keep);

    /**
     * @brief 调整 context 池到目标大小（可增可减）
     * @param target 目标 context 数量，0 表示不做任何调整
     */
    void resizeContextPool(size_t target);

    // 异步推理：使用提供的 CUDA stream 在 stream 完成时回调
    // cb(slot, success)
    bool asyncInfer(Slot* slot, cudaStream_t stream, std::function<void(Slot*, bool)> cb,
                    cudaEvent_t infer_end_event = nullptr);

    // 表示 engine 是否支持运行时动态设置 batch/shape
    bool supportsDynamicShape() const { return dynamic_shape_supported_; }

    // 统计：当前创建的 context 总数（创建-销毁）
    int getContextTotalCount() const {
        int created = ctx_created_.load(std::memory_order_relaxed);
        int destroyed = ctx_destroyed_.load(std::memory_order_relaxed);
        int total = created - destroyed;
        return total >= 0 ? total : 0;
    }

    /**
     * @brief 异步推理核心接口
     * @param slot    当前任务的 Slot (包含输入/输出显存地址)
     * @param context 调用线程私有的执行上下文
     * @param stream  调用线程私有的 CUDA 流
     * @return 异步发射是否成功
     */
    bool inference(Slot* slot, nvinfer1::IExecutionContext* context, cudaStream_t stream);

    // 获取模型输入尺寸，用于预处理参数设置
    int getInputH() const { return input_h_; }
    int getInputW() const { return input_w_; }
    size_t getOutputSize() const { return output_size_bytes_; }
    int getMaxBatch() const { return max_batch_; }
    size_t getOutputBytesPerBatch() const { return output_bytes_per_batch_; }

    // 解析检测结果：假设输出格式为 [batch, num_boxes, 6]，每个box: [x,y,w,h,conf,class]
    // 返回 vector<vector<Detection>> ，外层batch，内层detections
    struct Detection {
        float x, y, w, h, conf;
        int class_id;
    };
    std::vector<std::vector<Detection>> parseDetections(Slot* slot, float conf_threshold = 0.5f);

private:
    TRTDetector() = default;
    ~TRTDetector(); // 添加析构函数

    // TensorRT 核心组件
    std::unique_ptr<nvinfer1::IRuntime> runtime_;
    std::shared_ptr<nvinfer1::ICudaEngine> engine_;

    // 输入输出张量元数据
    std::string input_tensor_name_;
    std::string output_tensor_name_;
    int input_h_ = 0;
    int input_w_ = 0;
    size_t output_size_bytes_ = 0;
    size_t output_bytes_per_batch_ = 0;
    int max_batch_ = 1;
    int output_boxes_ = 0;
    int output_box_size_ = 0;

    std::atomic<int> ctx_created_{0};
    std::atomic<int> ctx_destroyed_{0};
    std::atomic<bool> shutting_down_{false};
    size_t context_pool_limit_ = 2; // 硬上限，避免 context 失控创建导致显存耗尽

    // 异步推理回调 in-flight 计数器：shutdown 前必须等待归零
    std::atomic<int> inflight_callbacks_{0};
    std::mutex inflight_mutex_;
    std::condition_variable inflight_cv_;

    // 上下文池与并发控制
    std::mutex ctx_mutex_;
    std::deque<nvinfer1::IExecutionContext*> context_pool_;
    bool dynamic_shape_supported_ = false;

    // 禁止拷贝
    TRTDetector(const TRTDetector&) = delete;
    TRTDetector& operator=(const TRTDetector&) = delete;
};

#endif
