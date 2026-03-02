#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <NvInfer.h>
#include <iostream>

// 继承自 TensorRT 定义的 ILogger 接口
class Logger : public nvinfer1::ILogger {
public:
    // severity: 日志等级（错误、警告、信息）
    // msg: 具体报错内容
    void log(Severity severity, const char* msg) noexcept override {
        // 我们只打印重要信息
        if (severity <= Severity::kINFO) {
            std::cout << "[TensorRT INFO]: " << msg << std::endl;
        }
    }
};

// 定义一个静态全局变量，方便到处调用
static Logger gLogger;

#endif
