#ifndef CUDA_IMAGE_PROC_CUH
#define CUDA_IMAGE_PROC_CUH

#include <cuda_runtime.h>
#include <cstdint>

// 启动 CUDA 核函数：将 NV12 (YUV420SP) 转换为 RGBA
// src_y: Y 平面设备指针
// src_uv: UV 平面设备指针
// src_pitch: 输入数据的行跨度 (字节)
// dst_rgba: 输出 RGBA 数据设备指针
// dst_pitch: 输出数据的行跨度 (字节)
void launchNV12ToRGBA(const uint8_t* src_y, const uint8_t* src_uv, int y_pitch, int uv_pitch,
                      uint8_t* dst_rgba, int dst_pitch, int width, int height);

// 将 RGBA (uint8_t) 图像 resize + letterbox 到目标 size，并写入目标 float NCHW (归一化到 [0,1])
// src_rgba: 输入 RGBA 图像设备指针
// src_pitch: 输入行跨度（字节）
// src_w, src_h: 输入宽高
// dst: 目标设备指针，布局为 CHW (float), 大小至少 3 * dst_w * dst_h
// dst_w, dst_h: 模型输入大小
// stream: CUDA stream 用于异步执行
void launchResizeLetterboxToFloatNCHW(const uint8_t* src_rgba, int src_pitch, int src_w, int src_h,
                                      float* dst, int dst_w, int dst_h, cudaStream_t stream);

// 直接从 NV12 的 device 平面（Y/UV）读取，做 resize+letterbox，并写入 float NCHW（归一化）
// 这是零拷贝版本，适用于输入已在 device 上的场景（例如 AV_PIX_FMT_CUDA）
void launchNV12ToFloatNCHWDevice(const uint8_t* dev_y, const uint8_t* dev_uv,
                                 int y_pitch, int uv_pitch, int src_w, int src_h,
                                 float* dst, int dst_w, int dst_h, cudaStream_t stream);

// 在 device 上将 NV12 缩放到目标分辨率（仍保持 NV12 格式）
// src_y/src_uv: 源 device 平面
// src_y_pitch/src_uv_pitch: 源行跨度
// dst_y/dst_uv: 目标 device 平面（预分配）
// dst_y_pitch/dst_uv_pitch: 目标行跨度（通常等于 dst_w）
void launchResizeNV12ToNV12Device(const uint8_t* src_y, const uint8_t* src_uv,
                                  int src_y_pitch, int src_uv_pitch, int src_w, int src_h,
                                  uint8_t* dst_y, uint8_t* dst_uv, int dst_y_pitch, int dst_uv_pitch,
                                  int dst_w, int dst_h, cudaStream_t stream);

#endif // CUDA_IMAGE_PROC_CUH