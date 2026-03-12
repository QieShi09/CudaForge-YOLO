#pragma once
#include <cuda_runtime.h>
#include <cstdint>

// 将输入图像 resize 并按比例填充到目标尺寸（letterbox），输入/输出均在设备内存。
// d_src: 原始图像（RGB24）
// d_dst: 目标图像（RGB24、大小 dst_w*dst_h*3）
void letterbox_cuda(const uint8_t* d_src, int src_w, int src_h, uint8_t* d_dst, int dst_w, int dst_h, cudaStream_t stream);
