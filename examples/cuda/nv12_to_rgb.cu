#include "nv12_to_rgb.cuh"
#include <cuda_runtime.h>
#include <cstdint>
#include <cstdio>

static __global__ void nv12_to_rgb_kernel(const uint8_t* y, const uint8_t* uv, int w, int h, uint8_t* rgb) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y_idx = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= w || y_idx >= h) return;

    int y_offset = y_idx * w + x;
    uint8_t Y = y[y_offset];

    int uv_x = x / 2;
    int uv_y = y_idx / 2;
    int uv_offset = uv_y * (w / 2) * 2 + uv_x * 2;
    uint8_t U = uv[uv_offset + 0];
    uint8_t V = uv[uv_offset + 1];

    // 简单转换（非最优、仅示例）
    int C = (int)Y - 16;
    int D = (int)U - 128;
    int E = (int)V - 128;
    int R = (298 * C + 409 * E + 128) >> 8;
    int G = (298 * C - 100 * D - 208 * E + 128) >> 8;
    int B = (298 * C + 516 * D + 128) >> 8;

    R = min(max(R, 0), 255);
    G = min(max(G, 0), 255);
    B = min(max(B, 0), 255);

    int out_offset = (y_idx * w + x) * 3;
    rgb[out_offset + 0] = (uint8_t)R;
    rgb[out_offset + 1] = (uint8_t)G;
    rgb[out_offset + 2] = (uint8_t)B;
}

void nv12_to_rgb_cuda(const uint8_t* d_y, const uint8_t* d_uv, int width, int height, uint8_t* d_rgb, cudaStream_t stream) {
    dim3 block(16, 16);
    dim3 grid((width + block.x - 1) / block.x, (height + block.y - 1) / block.y);
    nv12_to_rgb_kernel<<<grid, block, 0, stream>>>(d_y, d_uv, width, height, d_rgb);
}
