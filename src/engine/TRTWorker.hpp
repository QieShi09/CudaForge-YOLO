#ifndef TRT_WORKER_HPP
#define TRT_WORKER_HPP

#include <NvInfer.h>
#include <cuda_runtime.h>

#include <iostream>

#include "TRTDetector.hpp"

class Slot;

class TRTWorker {
public:
    explicit TRTWorker(int id) : id_(id) {}
    ~TRTWorker() { shutdown(); }

    bool init() {
        if (stream_ && context_) return true;

        cudaSetDevice(0);

        if (cudaStreamCreateWithFlags(&stream_, cudaStreamNonBlocking) != cudaSuccess || !stream_) {
            stream_ = nullptr;
            return false;
        }

        context_ = TRTDetector::getInstance().createContext();
        if (!context_) {
            cudaStreamDestroy(stream_);
            stream_ = nullptr;
            return false;
        }

        std::cout << "[TRTWorker] init ok, id=" << id_ << std::endl;
        return true;
    }

    void shutdown() {
        if (context_) {
            delete context_;
            context_ = nullptr;
        }
        if (stream_) {
            cudaStreamDestroy(stream_);
            stream_ = nullptr;
        }
    }

    cudaStream_t stream() const { return stream_; }
    nvinfer1::IExecutionContext* context() const { return context_; }

    bool enqueue(Slot* slot) {
        if (!slot || !context_ || !stream_) return false;
        return TRTDetector::getInstance().inference(slot, context_, stream_);
    }

private:
    int id_ = 0;
    cudaStream_t stream_ = nullptr;
    nvinfer1::IExecutionContext* context_ = nullptr;
};

#endif
