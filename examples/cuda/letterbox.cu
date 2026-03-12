#include "letterbox.cuh"
#include <cuda_runtime.h>
#include <algorithm>
#include <cstdint>

static __global__ void letterbox_kernel(const uint8_t* src, int sw, int sh, uint8_t* dst, int dw, int dh, float scale, int offx, int offy) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= dw || y >= dh) return;

    int dst_idx = (y * dw + x) * 3;

    // 计算对应源坐标
    int sx = (int)((x - offx) / scale);
    int sy = (int)((y - offy) / scale);
    if (sx < 0 || sx >= sw || sy < 0 || sy >= sh) {
        dst[dst_idx + 0] = 114; dst[dst_idx + 1] = 114; dst[dst_idx + 2] = 114; // 填充灰
        return;
    }
    int src_idx = (sy * sw + sx) * 3;
    dst[dst_idx + 0] = src[src_idx + 0];
    dst[dst_idx + 1] = src[src_idx + 1];
    dst[dst_idx + 2] = src[src_idx + 2];
}

void letterbox_cuda(const uint8_t* d_src, int src_w, int src_h, uint8_t* d_dst, int dst_w, int dst_h, cudaStream_t stream) {
    // 计算缩放与偏移（保持长宽比）
    float r_w = (float)dst_w / src_w;
    float r_h = (float)dst_h / src_h;
    float scale = (r_w < r_h) ? r_w : r_h;
    int new_w = (int)(src_w * scale);
    int new_h = (int)(src_h * scale);
    int offx = (dst_w - new_w) / 2;
    int offy = (dst_h - new_h) / 2;

    dim3 block(16, 16);
    dim3 grid((dst_w + block.x - 1) / block.x, (dst_h + block.y - 1) / block.y);
    letterbox_kernel<<<grid, block, 0, stream>>>(d_src, src_w, src_h, d_dst, dst_w, dst_h, scale, offx, offy);
}
