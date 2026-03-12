#include <iostream>
#include <cuda_runtime.h>
#include "nv12_to_rgb.cuh"

int main() {
    const int W = 640, H = 360;
    size_t y_size = W * H;
    size_t uv_size = (W/2) * (H/2) * 2;
    size_t rgb_size = W * H * 3;

    // host buffers (dummy data)
    uint8_t* h_y = (uint8_t*)malloc(y_size);
    uint8_t* h_uv = (uint8_t*)malloc(uv_size);
    for (size_t i=0;i<y_size;i++) h_y[i] = 128;
    for (size_t i=0;i<uv_size;i++) h_uv[i] = 128;

    uint8_t *d_y=nullptr, *d_uv=nullptr, *d_rgb=nullptr;
    cudaMalloc(&d_y, y_size);
    cudaMalloc(&d_uv, uv_size);
    cudaMalloc(&d_rgb, rgb_size);

    cudaMemcpy(d_y, h_y, y_size, cudaMemcpyHostToDevice);
    cudaMemcpy(d_uv, h_uv, uv_size, cudaMemcpyHostToDevice);

    nv12_to_rgb_cuda(d_y, d_uv, W, H, d_rgb, 0);

    uint8_t* h_rgb = (uint8_t*)malloc(rgb_size);
    cudaMemcpy(h_rgb, d_rgb, rgb_size, cudaMemcpyDeviceToHost);

    std::cout << "nv12_demo: first pixel RGB = " << (int)h_rgb[0] << "," << (int)h_rgb[1] << "," << (int)h_rgb[2] << "\n";

    free(h_y); free(h_uv); free(h_rgb);
    cudaFree(d_y); cudaFree(d_uv); cudaFree(d_rgb);
    return 0;
}
