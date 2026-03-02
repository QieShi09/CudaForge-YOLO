#include "CudaImageProc.cuh"
#include <cuda_runtime.h>

__device__ inline uint8_t clip_u8(float x) {
    return (x < 0.0f) ? 0 : ((x > 255.0f) ? 255 : (uint8_t)x);
}

__global__ void nv12_to_rgba_kernel(const uint8_t* src_y, const uint8_t* src_uv, int y_pitch, int uv_pitch,
                                    uint8_t* dst_rgba, int dst_pitch, int width, int height) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;

    if (x >= width || y >= height) return;

    // 读取 Y
    uint8_t y_val = src_y[y * y_pitch + x];

    // 读取 UV (NV12 格式下，UV 是交错存储的，且分辨率是 Y 的一半)
    int uv_x = x / 2;
    int uv_y = y / 2;
    int uv_offset = uv_y * uv_pitch + uv_x * 2;
    
    uint8_t u_val = src_uv[uv_offset];
    uint8_t v_val = src_uv[uv_offset + 1];

    // YUV 转 RGB (BT.601)
    float y_f = (float)y_val;
    float u_f = (float)u_val - 128.0f;
    float v_f = (float)v_val - 128.0f;

    float r = y_f + 1.402f * v_f;
    float g = y_f - 0.344136f * u_f - 0.714136f * v_f;
    float b = y_f + 1.772f * u_f;

    // 写入 RGBA (4字节)
    int dst_offset = y * dst_pitch + x * 4;
    dst_rgba[dst_offset + 0] = clip_u8(r);
    dst_rgba[dst_offset + 1] = clip_u8(g);
    dst_rgba[dst_offset + 2] = clip_u8(b);
    dst_rgba[dst_offset + 3] = 255; // Alpha
}

void launchNV12ToRGBA(const uint8_t* src_y, const uint8_t* src_uv, int y_pitch, int uv_pitch,
                      uint8_t* dst_rgba, int dst_pitch, int width, int height) {
    dim3 block(16, 16); // 降低 Block 大小，提高兼容性
    dim3 grid((width + block.x - 1) / block.x, (height + block.y - 1) / block.y);
    nv12_to_rgba_kernel<<<grid, block, 0, 0>>>(src_y, src_uv, y_pitch, uv_pitch, dst_rgba, dst_pitch, width, height); // 使用默认流 0
}

// 双线性采样函数
__device__ inline float bilinear_sample_channel(const uint8_t* src, int pitch, int src_w, int src_h, float fx, float fy, int channel_offset, int src_stride) {
    if (fx < 0 || fy < 0 || fx > src_w - 1 || fy > src_h - 1) return 0.0f;
    int x0 = (int)floorf(fx);
    int y0 = (int)floorf(fy);
    int x1 = min(x0 + 1, src_w - 1);
    int y1 = min(y0 + 1, src_h - 1);

    float dx = fx - x0;
    float dy = fy - y0;

    const uint8_t* p00 = src + y0 * pitch + x0 * src_stride + channel_offset;
    const uint8_t* p10 = src + y0 * pitch + x1 * src_stride + channel_offset;
    const uint8_t* p01 = src + y1 * pitch + x0 * src_stride + channel_offset;
    const uint8_t* p11 = src + y1 * pitch + x1 * src_stride + channel_offset;

    float v00 = (float)(*p00);
    float v10 = (float)(*p10);
    float v01 = (float)(*p01);
    float v11 = (float)(*p11);

    float v0 = v00 * (1 - dx) + v10 * dx;
    float v1 = v01 * (1 - dx) + v11 * dx;
    return v0 * (1 - dy) + v1 * dy;
}

// 将 RGBA uchar 图像 resize + letterbox 到 float NCHW
__global__ void resize_letterbox_to_nchw_kernel(const uint8_t* src_rgba, int src_pitch, int src_w, int src_h,
                                                float* dst, int dst_w, int dst_h)
{
    int dx = blockIdx.x * blockDim.x + threadIdx.x;
    int dy = blockIdx.y * blockDim.y + threadIdx.y;
    if (dx >= dst_w || dy >= dst_h) return;

    // 计算缩放与填充
    float scale_x = (float)dst_w / (float)src_w;
    float scale_y = (float)dst_h / (float)src_h;
    float scale = fminf(scale_x, scale_y);
    int new_w = (int)floorf(src_w * scale + 0.5f);
    int new_h = (int)floorf(src_h * scale + 0.5f);
    int pad_x = (dst_w - new_w) / 2;
    int pad_y = (dst_h - new_h) / 2;

    // 目标坐标对应源坐标
    float fx = (dx - pad_x + 0.5f) / scale - 0.5f;
    float fy = (dy - pad_y + 0.5f) / scale - 0.5f;

    // 若在 padding 区，填充为 0
    float r = 0.0f, g = 0.0f, b = 0.0f;
    if (fx >= 0.0f && fy >= 0.0f && fx <= src_w - 1 && fy <= src_h - 1) {
        // src_rgba 按字节排列 RGBA
        r = bilinear_sample_channel(src_rgba, src_pitch, src_w, src_h, fx, fy, 0, 4) / 255.0f;
        g = bilinear_sample_channel(src_rgba, src_pitch, src_w, src_h, fx, fy, 1, 4) / 255.0f;
        b = bilinear_sample_channel(src_rgba, src_pitch, src_w, src_h, fx, fy, 2, 4) / 255.0f;
    }

    // 写入 NCHW 布局： channel 0 = R plane offset = 0, channel 1 offset = dst_w*dst_h, channel2 = 2*...
    int pix_idx = dy * dst_w + dx;
    int plane = dst_w * dst_h;
    dst[0 * plane + pix_idx] = r;
    dst[1 * plane + pix_idx] = g;
    dst[2 * plane + pix_idx] = b;
}

void launchResizeLetterboxToFloatNCHW(const uint8_t* src_rgba, int src_pitch, int src_w, int src_h,
                                      float* dst, int dst_w, int dst_h, cudaStream_t stream)
{
    dim3 block(16, 16);
    dim3 grid((dst_w + block.x - 1) / block.x, (dst_h + block.y - 1) / block.y);
    resize_letterbox_to_nchw_kernel<<<grid, block, 0, stream>>>(src_rgba, src_pitch, src_w, src_h, dst, dst_w, dst_h);
}

// 直接从 NV12 device planes 生成 float NCHW（归一化）
__global__ void nv12_to_float_nchw_kernel(const uint8_t* dev_y, const uint8_t* dev_uv,
                                          int y_pitch, int uv_pitch, int src_w, int src_h,
                                          float* dst, int dst_w, int dst_h)
{
    int dx = blockIdx.x * blockDim.x + threadIdx.x;
    int dy = blockIdx.y * blockDim.y + threadIdx.y;
    if (dx >= dst_w || dy >= dst_h) return;

    // 计算缩放与填充
    float scale_x = (float)dst_w / (float)src_w;
    float scale_y = (float)dst_h / (float)src_h;
    float scale = fminf(scale_x, scale_y);
    int new_w = (int)floorf(src_w * scale + 0.5f);
    int new_h = (int)floorf(src_h * scale + 0.5f);
    int pad_x = (dst_w - new_w) / 2;
    int pad_y = (dst_h - new_h) / 2;

    // 目标坐标对应源坐标
    float fx = (dx - pad_x + 0.5f) / scale - 0.5f;
    float fy = (dy - pad_y + 0.5f) / scale - 0.5f;

    float r = 0.0f, g = 0.0f, b = 0.0f;
    if (fx >= 0.0f && fy >= 0.0f && fx <= src_w - 1 && fy <= src_h - 1) {
        // 双线性采样 Y
        int x0 = (int)floorf(fx);
        int y0 = (int)floorf(fy);
        int x1 = min(x0 + 1, src_w - 1);
        int y1 = min(y0 + 1, src_h - 1);
        float dxr = fx - x0;
        float dyr = fy - y0;

        float Y00 = (float)dev_y[y0 * y_pitch + x0];
        float Y10 = (float)dev_y[y0 * y_pitch + x1];
        float Y01 = (float)dev_y[y1 * y_pitch + x0];
        float Y11 = (float)dev_y[y1 * y_pitch + x1];
        float Y0 = Y00 * (1 - dxr) + Y10 * dxr;
        float Y1 = Y01 * (1 - dxr) + Y11 * dxr;
        float Y = Y0 * (1 - dyr) + Y1 * dyr;

        // 对 UV 采样（注意 UV 分辨率为 Y 的一半，采用最邻近采样以简单化）
        int uv_x = min(src_w/2 - 1, max(0, (int)roundf(fx/2.0f)));
        int uv_y = min(src_h/2 - 1, max(0, (int)roundf(fy/2.0f)));
        int uv_off = uv_y * uv_pitch + uv_x * 2;
        float U = (float)dev_uv[uv_off];
        float V = (float)dev_uv[uv_off + 1];

        // YUV -> RGB (BT.601)
        float y_f = Y;
        float u_f = U - 128.0f;
        float v_f = V - 128.0f;
        float rf = y_f + 1.402f * v_f;
        float gf = y_f - 0.344136f * u_f - 0.714136f * v_f;
        float bf = y_f + 1.772f * u_f;

        r = rf / 255.0f;
        g = gf / 255.0f;
        b = bf / 255.0f;
    }

    int pix_idx = dy * dst_w + dx;
    int plane = dst_w * dst_h;
    dst[0 * plane + pix_idx] = r;
    dst[1 * plane + pix_idx] = g;
    dst[2 * plane + pix_idx] = b;
}

void launchNV12ToFloatNCHWDevice(const uint8_t* dev_y, const uint8_t* dev_uv,
                                 int y_pitch, int uv_pitch, int src_w, int src_h,
                                 float* dst, int dst_w, int dst_h, cudaStream_t stream)
{
    dim3 block(16,16);
    dim3 grid((dst_w + block.x - 1)/block.x, (dst_h + block.y - 1)/block.y);
    nv12_to_float_nchw_kernel<<<grid, block, 0, stream>>>(dev_y, dev_uv, y_pitch, uv_pitch, src_w, src_h, dst, dst_w, dst_h);
}

// 简单的 NV12 Y 平面双线性缩放内核（写入目标 Y 平面）
__global__ void nv12_resize_y_kernel(const uint8_t* src_y, int src_pitch, int src_w, int src_h,
                                     uint8_t* dst_y, int dst_pitch, int dst_w, int dst_h)
{
    int dx = blockIdx.x * blockDim.x + threadIdx.x;
    int dy = blockIdx.y * blockDim.y + threadIdx.y;
    if (dx >= dst_w || dy >= dst_h) return;

    float scale_x = (float)dst_w / (float)src_w;
    float scale_y = (float)dst_h / (float)src_h;
    float scale = fminf(scale_x, scale_y);
    int new_w = (int)floorf(src_w * scale + 0.5f);
    int new_h = (int)floorf(src_h * scale + 0.5f);
    int pad_x = (dst_w - new_w) / 2;
    int pad_y = (dst_h - new_h) / 2;

    // 目标坐标对应源坐标
    float fx = (dx - pad_x + 0.5f) / scale - 0.5f;
    float fy = (dy - pad_y + 0.5f) / scale - 0.5f;

    uint8_t out = 0;
    if (fx >= 0.0f && fy >= 0.0f && fx <= src_w - 1 && fy <= src_h - 1) {
        int x0 = (int)floorf(fx);
        int y0 = (int)floorf(fy);
        int x1 = min(x0 + 1, src_w - 1);
        int y1 = min(y0 + 1, src_h - 1);
        float dxr = fx - x0;
        float dyr = fy - y0;

        float v00 = (float)src_y[y0 * src_pitch + x0];
        float v10 = (float)src_y[y0 * src_pitch + x1];
        float v01 = (float)src_y[y1 * src_pitch + x0];
        float v11 = (float)src_y[y1 * src_pitch + x1];
        float v0 = v00 * (1 - dxr) + v10 * dxr;
        float v1 = v01 * (1 - dxr) + v11 * dxr;
        float v = v0 * (1 - dyr) + v1 * dyr;
        out = (uint8_t)fminf(fmaxf(v, 0.0f), 255.0f);
    }

    dst_y[dy * dst_pitch + dx] = out;
}

// NV12 UV 平面缩放（目标为 interleaved UV）
__global__ void nv12_resize_uv_kernel(const uint8_t* src_uv, int src_uv_pitch, int src_w, int src_h,
                                      uint8_t* dst_uv, int dst_uv_pitch, int dst_w, int dst_h)
{
    // UV 的尺寸为 src_w/2 x src_h/2
    int dst_uv_w = dst_w / 2;
    int dst_uv_h = dst_h / 2;
    int dx = blockIdx.x * blockDim.x + threadIdx.x;
    int dy = blockIdx.y * blockDim.y + threadIdx.y;
    if (dx >= dst_uv_w || dy >= dst_uv_h) return;

    float scale_x = (float)dst_w / (float)src_w;
    float scale_y = (float)dst_h / (float)src_h;
    float scale = fminf(scale_x, scale_y);
    int new_w = (int)floorf(src_w * scale + 0.5f);
    int new_h = (int)floorf(src_h * scale + 0.5f);
    int pad_x = (dst_w - new_w) / 2;
    int pad_y = (dst_h - new_h) / 2;

    // 对应到 UV 平面坐标
    float fx = ((dx * 2) - pad_x + 0.5f) / scale - 0.5f;
    float fy = ((dy * 2) - pad_y + 0.5f) / scale - 0.5f;

    uint8_t u=128, v=128;
    if (fx >= 0.0f && fy >= 0.0f && fx <= src_w - 1 && fy <= src_h - 1) {
        int src_uv_w = src_w / 2;
        int src_uv_h = src_h / 2;
        int ux = min(src_uv_w - 1, max(0, (int)roundf(fx/2.0f)));
        int uy = min(src_uv_h - 1, max(0, (int)roundf(fy/2.0f)));
        int off = uy * src_uv_pitch + ux * 2;
        u = src_uv[off];
        v = src_uv[off + 1];
    }

    int dst_off = dy * dst_uv_pitch + dx * 2;
    dst_uv[dst_off] = u;
    dst_uv[dst_off + 1] = v;
}

void launchResizeNV12ToNV12Device(const uint8_t* src_y, const uint8_t* src_uv,
                                  int src_y_pitch, int src_uv_pitch, int src_w, int src_h,
                                  uint8_t* dst_y, uint8_t* dst_uv, int dst_y_pitch, int dst_uv_pitch,
                                  int dst_w, int dst_h, cudaStream_t stream)
{
    // Y plane
    dim3 by(16,16);
    dim3 gy((dst_w + by.x - 1)/by.x, (dst_h + by.y - 1)/by.y);
    nv12_resize_y_kernel<<<gy, by, 0, stream>>>(src_y, src_y_pitch, src_w, src_h, dst_y, dst_y_pitch, dst_w, dst_h);

    // UV plane: UV target dimensions are dst_w/2 x dst_h/2
    dim3 bu(16,16);
    int dst_uv_w = dst_w/2;
    int dst_uv_h = dst_h/2;
    dim3 gu((dst_uv_w + bu.x - 1)/bu.x, (dst_uv_h + bu.y - 1)/bu.y);
    nv12_resize_uv_kernel<<<gu, bu, 0, stream>>>(src_uv, src_uv_pitch, src_w, src_h, dst_uv, dst_uv_pitch, dst_w, dst_h);
}