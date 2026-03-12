#pragma once
#include <cuda_runtime.h>
#include <cstdint>

// 将 NV12 (Y plane + interleaved UV) 转为 RGB24，输入输出均在 GPU 显存。
// d_y: 指向 Y 平面的设备指针
// d_uv: 指向 UV 平面的设备指针
// d_rgb: 输出 RGB24 的设备指针（行主）
void nv12_to_rgb_cuda(const uint8_t* d_y, const uint8_t* d_uv, int width, int height, uint8_t* d_rgb, cudaStream_t stream);
