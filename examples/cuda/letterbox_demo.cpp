#include <iostream>
#include <cuda_runtime.h>
#include "letterbox.cuh"

int main() {
    const int SW = 320, SH = 180;
    const int DW = 640, DH = 640;
    size_t src_size = SW * SH * 3;
    size_t dst_size = DW * DH * 3;

    uint8_t* h_src = (uint8_t*)malloc(src_size);
    for (size_t i=0;i<src_size;i++) h_src[i] = (uint8_t)(i & 255);

    uint8_t *d_src=nullptr, *d_dst=nullptr;
    cudaMalloc(&d_src, src_size);
    cudaMalloc(&d_dst, dst_size);
    cudaMemcpy(d_src, h_src, src_size, cudaMemcpyHostToDevice);

    letterbox_cuda(d_src, SW, SH, d_dst, DW, DH, 0);

    uint8_t* h_dst = (uint8_t*)malloc(dst_size);
    cudaMemcpy(h_dst, d_dst, dst_size, cudaMemcpyDeviceToHost);
    std::cout << "letterbox_demo: sample dst pixel = " << (int)h_dst[0] << "," << (int)h_dst[1] << "," << (int)h_dst[2] << "\n";

    free(h_src); free(h_dst);
    cudaFree(d_src); cudaFree(d_dst);
    return 0;
}
