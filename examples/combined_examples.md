# CudaForge-YOLO Examples 汇总

本文件将 `examples` 目录下的 README 与关键示例源码合并，便于使用 pandoc 导出为 PDF 在手机上阅读。

说明：若要将此文件转换为 PDF，请在系统安装 `pandoc` 与 LaTeX（如 `texlive-xetex`）后运行：

```bash
cd examples
pandoc -s combined_examples.md -o CudaForgeExamples.pdf --highlight-style=tango --pdf-engine=xelatex
```

---

---

# 简历版本（全部文件）

下面是每个示例的详细注释版本，采用"面试官问：..."的形式，解释每段代码的作用与设计思路。

## hw_decode_demo.cpp （面试官：说一下硬件解码的完整流程）

cpp

```
/*
 * 面试官问：说一下FFmpeg硬件解码的完整流程
 * 
 * 解码器初始化的完整流程是这样的：
 * 
 * 1. 打开文件获取格式上下文：从视频文件中读取容器信息
 * 
 * 2. 找到视频流：遍历所有流，找到第一个视频流
 * 
 * 3. 获取解码参数：从视频流中拿到编码参数（codecpar）
 * 
 * 4. 查找解码器：根据编码ID找到对应的解码器
 * 
 * 5. 创建解码器上下文：分配内存空间
 * 
 * 6. 复制参数：把流的编码参数告诉解码器
 * 
 * 7. 配置硬件加速（如果启用）：
 *    7.1 hw_device_ctx：创建CUDA硬件设备，建立GPU连接
 *    7.2 get_format：设置回调，让解码器输出CUDA格式的帧
 *    7.3 可选配置hw_frames_ctx：管理硬件帧池
 * 
 * 8. 打开解码器：完成初始化，准备解码
 */
#include <iostream>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libavutil/hwcontext.h>
#include <libswscale/swscale.h>
}

// 7.2 get_format：设置回调，让解码器输出CUDA格式的帧
// 当解码器初始化时，会调用这个函数询问支持的像素格式
static enum AVPixelFormat get_hw_format(AVCodecContext *ctx, const enum AVPixelFormat *pix_fmts) {
    for (const enum AVPixelFormat *p = pix_fmts; *p != -1; p++) {
        return *p; // 实际项目中应该检测并返回硬件格式，如AV_PIX_FMT_CUDA
    }
    return AV_PIX_FMT_NONE;
}

bool open_input(const char* filename, AVFormatContext** outFmtCtx, AVCodecContext** outDecCtx, int* outVideoStream) {
    av_log_set_level(AV_LOG_ERROR);
    AVFormatContext* fmt = nullptr;
    
    // 1. 打开文件获取格式上下文：avformat_open_input读取文件头，avformat_find_stream_info读取流信息
    if (avformat_open_input(&fmt, filename, nullptr, nullptr) < 0) {
        std::cerr << "Failed to open input: " << filename << "\n";
        return false;
    }
    if (avformat_find_stream_info(fmt, nullptr) < 0) {
        std::cerr << "Failed to find stream info\n";
        avformat_close_input(&fmt);
        return false;
    }
    
    // 2. 找到视频流：遍历所有流，找到类型为AVMEDIA_TYPE_VIDEO的流
    int video_stream = -1;
    for (unsigned i = 0; i < fmt->nb_streams; ++i) {
        if (fmt->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) { 
            video_stream = i; 
            break; 
        }
    }
    if (video_stream < 0) { 
        std::cerr << "No video stream found\n"; 
        avformat_close_input(&fmt); 
        return false; 
    }

    // 3. 获取解码参数：从视频流中拿到编码参数（编码器ID、分辨率、像素格式等）
    AVCodecParameters* par = fmt->streams[video_stream]->codecpar;
    
    // 4. 查找解码器：根据编码ID找到对应的解码器
    const AVCodec* decoder = avcodec_find_decoder(par->codec_id);
    if (!decoder) { 
        std::cerr << "Decoder not found\n"; 
        avformat_close_input(&fmt); 
        return false; 
    }

    // 5. 创建解码器上下文：分配内存空间，创建解码器的工作环境
    AVCodecContext* dec_ctx = avcodec_alloc_context3(decoder);
    if (!dec_ctx) { 
        std::cerr << "Failed to alloc codec context\n"; 
        avformat_close_input(&fmt); 
        return false; 
    }
    
    // 6. 复制参数：把流的编码参数告诉解码器
    if (avcodec_parameters_to_context(dec_ctx, par) < 0) { 
        std::cerr << "Failed to copy codec params\n"; 
        avcodec_free_context(&dec_ctx); 
        avformat_close_input(&fmt); 
        return false; 
    }

    // 7. 配置硬件加速（如果启用）：这里展示的是配置代码，实际使用时需要解注释
    /*
    // 7.1 hw_device_ctx：创建CUDA硬件设备，建立GPU连接
    AVBufferRef* hw_device_ctx = nullptr;
    av_hwdevice_ctx_create(&hw_device_ctx, AV_HWDEVICE_TYPE_CUDA, nullptr, nullptr, 0);
    dec_ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);
    
    // 7.2 get_format：设置回调，让解码器输出CUDA格式的帧
    dec_ctx->get_format = get_hw_format;
    
    // 7.3 可选配置hw_frames_ctx：管理硬件帧池
    AVHWFramesContext* hw_frames_ctx = av_hwframe_ctx_alloc(hw_device_ctx);
    hw_frames_ctx->format = AV_PIX_FMT_CUDA;
    hw_frames_ctx->sw_format = AV_PIX_FMT_NV12;
    hw_frames_ctx->width = dec_ctx->width;
    hw_frames_ctx->height = dec_ctx->height;
    hw_frames_ctx->initial_pool_size = 20;
    av_hwframe_ctx_init(hw_frames_ctx);
    dec_ctx->hw_frames_ctx = av_buffer_ref(hw_frames_ctx);
    */

    // 8. 打开解码器：完成初始化，准备解码
    if (avcodec_open2(dec_ctx, decoder, nullptr) < 0) { 
        std::cerr << "Failed to open codec\n"; 
        avcodec_free_context(&dec_ctx); 
        avformat_close_input(&fmt); 
        return false; 
    }

    *outFmtCtx = fmt; 
    *outDecCtx = dec_ctx; 
    *outVideoStream = video_stream;
    return true;
}

/*
 * 面试官问：解码循环是怎么工作的？avcodec_send_packet和avcodec_receive_frame是什么关系？
 * 
 * 解码循环采用"生产者-消费者"模式：
 * 
 * 1. av_read_frame：从容器中读取一个压缩数据包（Packet）
 * 
 * 2. avcodec_send_packet：把压缩包送入解码器（生产者）
 *    - 可能因为解码器内部缓冲满了而返回-EAGAIN
 *    - 传NULL表示冲刷解码器，获取剩余帧
 * 
 * 3. avcodec_receive_frame：从解码器取出解压后的帧（消费者）
 *    - 可能一次send对应多次receive（比如B帧）
 *    - 返回0表示成功拿到一帧
 *    - 返回-EAGAIN表示需要更多数据
 * 
 * 4. 处理帧：这里打印PTS（显示时间戳）和像素格式
 * 
 * 5. 释放包：av_packet_unref释放包内数据引用
 */
void decode_loop(AVFormatContext* fmtCtx, AVCodecContext* decCtx, int video_stream_index, int max_frames) {
    AVPacket* pkt = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    AVFrame* sw_frame = av_frame_alloc();
    int count = 0;

    while (av_read_frame(fmtCtx, pkt) >= 0 && count < max_frames) {
        if (pkt->stream_index == video_stream_index) {
            // 2. avcodec_send_packet：送入压缩包
            if (avcodec_send_packet(decCtx, pkt) == 0) {
                // 3. avcodec_receive_frame：取出解压帧
                while (avcodec_receive_frame(decCtx, frame) == 0) {
                    // 4. 处理帧：如果是硬件帧，frame->format会是硬件格式
                    if (frame->format == AV_PIX_FMT_NONE) {
                        std::cout << "Received a hardware frame (unsupported format id).\n";
                    }
                    AVRational tb = fmtCtx->streams[video_stream_index]->time_base;
                    double pts_seconds = frame->best_effort_timestamp == AV_NOPTS_VALUE ? 0 : frame->best_effort_timestamp * av_q2d(tb);
                    std::cout << "Frame " << count << " pts=" << frame->best_effort_timestamp << " (" << pts_seconds << "s) format=" << frame->format << "\n";
                    ++count;
                    if (count >= max_frames) break;
                }
            }
        }
        // 5. 释放包：av_packet_unref减少引用计数
        av_packet_unref(pkt);
    }

    // 发送空包冲刷解码器，获取剩余帧
    avcodec_send_packet(decCtx, nullptr);
    while (avcodec_receive_frame(decCtx, frame) == 0) {
        std::cout << "Flushed frame format=" << frame->format << "\n";
    }

    av_frame_free(&frame);
    av_frame_free(&sw_frame);
    av_packet_free(&pkt);
}

int main(int argc, char** argv) {
    if (argc < 2) { std::cout << "Usage: hw_decode_demo <video>\n"; return 0; }
    AVFormatContext* fmt = nullptr; 
    AVCodecContext* dec = nullptr; 
    int vid = -1;
    
    if (!open_input(argv[1], &fmt, &dec, &vid)) return 1;
    std::cout << "Opened " << argv[1] << ", decoding 10 frames...\n";
    decode_loop(fmt, dec, vid, 10);
    
    avcodec_free_context(&dec);
    avformat_close_input(&fmt);
    return 0;
}
```



## seek_demo.cpp （面试官：说一下FFmpeg的seek实现原理）

cpp

```
/*
 * 面试官问：FFmpeg的seek是怎么实现的？时间基转换是怎么回事？
 * 
 * Seek的实现流程：
 * 
 * 1. 时间基转换：不同的流有自己的时间基，需要把目标时间（毫秒）转换到流的时间基
 *    - av_rescale_q：在不同时间基之间换算数值
 *    - 公式：目标时间戳 = 毫秒数 × (流时间基倒数/1000)
 * 
 * 2. av_seek_frame：执行seek操作
 *    - AVSEEK_FLAG_BACKWARD：向前寻找最近的关键帧（保证能找到）
 *    - AVSEEK_FLAG_ANY：允许定位到非关键帧（可能产生马赛克）
 *    - AVSEEK_FLAG_BYTE：按字节位置seek（用于直播流）
 * 
 * 3. avcodec_flush_buffers：刷新解码器内部缓冲
 *    - 清除已解码但未取出的帧
 *    - 重置解码器的状态，准备从新位置开始解码
 */

#include <iostream>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

/*
 * seek_to_timestamp：按毫秒时间戳定位
 * 
 * 参数：
 *   fmtCtx：格式上下文
 *   stream_index：要seek的视频流索引
 *   timestamp_ms：目标时间（毫秒）
 * 
 * 返回：成功返回true，失败返回false
 */
bool seek_to_timestamp(AVFormatContext* fmtCtx, int stream_index, int64_t timestamp_ms) {
    if (!fmtCtx || stream_index < 0) return false;
    AVStream* st = fmtCtx->streams[stream_index];
    
    // 1. 时间基转换：将毫秒(1/1000)转换为流的时间基
    // av_rescale_q(a, b, c) = a * b / c
    int64_t ts = av_rescale_q(timestamp_ms, (AVRational){1,1000}, st->time_base);
    
    // 2. av_seek_frame：执行seek操作
    // AVSEEK_FLAG_BACKWARD：向前寻找，确保能找到关键帧
    if (av_seek_frame(fmtCtx, stream_index, ts, AVSEEK_FLAG_BACKWARD) < 0) {
        std::cerr << "seek failed\n";
        return false;
    }
    
    // 3. avcodec_flush_buffers：刷新解码器缓冲
    // 注意：这里直接访问codec字段在新版FFmpeg中已废弃
    // 正确做法是保存解码器上下文并调用avcodec_flush_buffers
    avcodec_flush_buffers(fmtCtx->streams[stream_index]->codec);
    return true;
}

int main(int argc, char** argv) {
    if (argc < 3) { 
        std::cout << "Usage: seek_demo <video> <seek_ms>\n"; 
        return 0; 
    }
    
    const char* fn = argv[1];
    int64_t seek_ms = atoll(argv[2]);
    
    AVFormatContext* fmt = nullptr;
    
    // 打开文件
    if (avformat_open_input(&fmt, fn, nullptr, nullptr) < 0) { 
        std::cerr << "open failed\n"; 
        return 1; 
    }
    
    // 查找流信息
    if (avformat_find_stream_info(fmt, nullptr) < 0) { 
        std::cerr << "find stream info failed\n"; 
        avformat_close_input(&fmt); 
        return 1; 
    }
    
    // 找到视频流
    int vindex = -1;
    for (unsigned i=0;i<fmt->nb_streams;i++) {
        if (fmt->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) { 
            vindex = i; 
            break; 
        }
    }
    if (vindex < 0) { 
        std::cerr << "no video stream\n"; 
        avformat_close_input(&fmt); 
        return 1; 
    }

    // 执行seek
    if (!seek_to_timestamp(fmt, vindex, seek_ms)) { 
        std::cerr << "seek failed\n"; 
        avformat_close_input(&fmt); 
        return 1; 
    }
    
    std::cout << "seeked to " << seek_ms << " ms (stream " << vindex << ")\n";
    avformat_close_input(&fmt);
    return 0;
}
```



## speed_demo.cpp （面试官：说一下播放器的倍速控制原理）

cpp

```
/*
 * 面试官问：播放器的倍速控制是怎么实现的？
 * 
 * 倍速播放的核心原理：
 * 
 * 1. 帧率固定：视频源有固定的帧率（比如25fps），每帧间隔40ms
 * 
 * 2. 倍速调整：倍速播放就是改变帧的显示间隔
 *    - 2倍速：显示间隔减半（40ms → 20ms）
 *    - 0.5倍速：显示间隔加倍（40ms → 80ms）
 * 
 * 3. 实现方式：
 *    - 方式一：调整sleep时间（本示例采用）
 *    - 方式二：丢帧或重复帧（不改变sleep，通过跳帧实现）
 *    - 方式三：调整音频重采样（音频变调不变速需要特殊处理）
 * 
 * 4. 注意事项：
 *    - 解码速度要跟上播放速度（否则会卡顿）
 *    - 音频倍速需要重采样和变调处理
 *    - 暂停是倍速为0的特殊情况
 */

#include <iostream>
#include <thread>
#include <chrono>

class PlaybackController {
public:
    PlaybackController(): speed_(1.0), paused_(false) {}
    
    // 设置倍速：2.0表示2倍速，0.5表示0.5倍速
    void set_speed(double s) { speed_ = s; }
    double speed() const { return speed_; }
    
    // 播放控制
    void play() { paused_ = false; }
    void pause() { paused_ = true; }
    bool paused() const { return paused_; }
    
private:
    double speed_;  // 当前倍速
    bool paused_;   // 暂停状态
};

int main() {
    PlaybackController ctrl;
    double frame_rate = 25.0;           // 假设源视频是25 FPS
    double frame_interval_ms = 1000.0 / frame_rate;  // 每帧间隔40ms
    int frame = 0;
    
    // 设置为2倍速：每帧显示时间减半
    ctrl.set_speed(2.0);

    for (;;) {
        if (!ctrl.paused()) {
            // 实际项目中，这里应该是解码、渲染一帧
            std::cout << "Displaying frame " << frame << " at speed=" << ctrl.speed() << "x\n";
            ++frame;
        }
        
        // 根据倍速调整等待时间：倍速越高，等待越短
        // wait_ms = 原始帧间隔 / 倍速
        double wait_ms = frame_interval_ms / ctrl.speed();
        std::this_thread::sleep_for(std::chrono::milliseconds((int)wait_ms));
        
        if (frame >= 100) break;
    }
    return 0;
}
```



## nv12_to_[rgb.cu](https://rgb.cu/) （面试官：说一下NV12到RGB的转换原理和CUDA实现）

cpp

```
/*
 * 面试官问：NV12格式是什么？怎么在CUDA上转换成RGB？
 * 
 * NV12格式解析：
 * 
 * 1. NV12是YUV色彩空间的一种平面格式：
 *    - Y平面：单独存储亮度信息，每个像素一个字节
 *    - UV平面：交错存储色度信息，每2x2像素共用一组U和V
 * 
 * 2. 内存布局：
 *    - Y平面：大小为 width × height 字节
 *    - UV平面：大小为 (width/2) × (height/2) × 2 字节
 *      (每对U和V交错存储，即U,V,U,V,...)
 * 
 * 3. YUV到RGB转换公式（BT.601标准）：
 *    Y' = 16~235, U/V = 16~240（视频范围）
 *    转换前需要偏移：Y-16, U-128, V-128
 *    
 *    R = 1.164*(Y-16) + 1.596*(V-128)
 *    G = 1.164*(Y-16) - 0.391*(U-128) - 0.813*(V-128)
 *    B = 1.164*(Y-16) + 2.018*(U-128)
 *    
 *    整数实现使用定点数：乘以256再右移8位
 * 
 * CUDA实现要点：
 * 
 * 1. 每个线程处理一个像素：避免重复读取UV
 * 2. UV索引计算：uv_x = x/2, uv_y = y/2
 * 3. 使用min/max裁剪到0-255范围
 * 4. 支持CUDA流实现异步执行
 */

#include "nv12_to_rgb.cuh"
#include <cuda_runtime.h>
#include <cstdint>
#include <cstdio>

/*
 * nv12_to_rgb_kernel：CUDA核函数，每个线程处理一个像素
 * 
 * 线程映射：每个线程对应输出图像中的一个像素(x,y)
 * 
 * 参数：
 *   y：Y平面设备指针
 *   uv：UV平面设备指针（交错存储）
 *   w,h：图像宽高
 *   rgb：输出RGB24设备指针
 */
static __global__ void nv12_to_rgb_kernel(const uint8_t* y, const uint8_t* uv, int w, int h, uint8_t* rgb) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y_idx = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= w || y_idx >= h) return;

    // 1. 读取Y值：每个像素独立的Y
    int y_offset = y_idx * w + x;
    uint8_t Y = y[y_offset];

    // 2. 计算UV索引：每2x2像素共享一组UV
    int uv_x = x / 2;
    int uv_y = y_idx / 2;
    // UV平面布局：每行有(w/2)组UV，每组2字节(U,V)
    int uv_offset = uv_y * (w / 2) * 2 + uv_x * 2;
    uint8_t U = uv[uv_offset + 0];
    uint8_t V = uv[uv_offset + 1];

    // 3. YUV转RGB（整数近似实现）
    // 减去偏移量：Y偏移16，U/V偏移128
    int C = (int)Y - 16;
    int D = (int)U - 128;
    int E = (int)V - 128;
    
    // 定点数计算：系数乘以256，结果右移8位
    int R = (298 * C + 409 * E + 128) >> 8;
    int G = (298 * C - 100 * D - 208 * E + 128) >> 8;
    int B = (298 * C + 516 * D + 128) >> 8;

    // 4. 裁剪到合法范围[0,255]
    R = min(max(R, 0), 255);
    G = min(max(G, 0), 255);
    B = min(max(B, 0), 255);

    // 5. 写入RGB（RGB24格式：R,G,B连续存储）
    int out_offset = (y_idx * w + x) * 3;
    rgb[out_offset + 0] = (uint8_t)R;
    rgb[out_offset + 1] = (uint8_t)G;
    rgb[out_offset + 2] = (uint8_t)B;
}

/*
 * nv12_to_rgb_cuda：主机端调用函数，启动CUDA核函数
 * 
 * 参数：
 *   d_y：Y平面设备指针
 *   d_uv：UV平面设备指针
 *   width,height：图像尺寸
 *   d_rgb：输出RGB设备指针
 *   stream：CUDA流，用于异步执行
 * 
 * 启动配置：
 *   block：16x16线程（256线程/块）
 *   grid：根据图像大小计算所需块数
 */
void nv12_to_rgb_cuda(const uint8_t* d_y, const uint8_t* d_uv, int width, int height, uint8_t* d_rgb, cudaStream_t stream) {
    dim3 block(16, 16);
    dim3 grid((width + block.x - 1) / block.x, (height + block.y - 1) / block.y);
    
    // 在指定流上启动核函数
    nv12_to_rgb_kernel<<<grid, block, 0, stream>>>(d_y, d_uv, width, height, d_rgb);
}
```



## [letterbox.cu](https://letterbox.cu/) （面试官：说一下YOLO的letterbox预处理原理）

cpp

```
/*
 * 面试官问：YOLO为什么要做letterbox？怎么在CUDA上实现？
 * 
 * Letterbox原理：
 * 
 * 1. 为什么要做letterbox：
 *    - YOLO网络输入是固定尺寸（如640x640）
 *    - 直接resize会改变长宽比，导致目标变形
 *    - letterbox保持原图长宽比，多余部分填充灰色（114）
 * 
 * 2. 实现步骤：
 *    2.1 计算缩放比例：r_w = dst_w/src_w, r_h = dst_h/src_h
 *    2.2 选择较小比例：scale = min(r_w, r_h) 保证完整显示
 *    2.3 计算缩放后尺寸：new_w = src_w*scale, new_h = src_h*scale
 *    2.4 计算偏移量：offx = (dst_w - new_w)/2, offy = (dst_h - new_h)/2
 * 
 * 3. CUDA实现特点：
 *    - 反向映射：每个目标像素找对应的源像素（避免空洞）
 *    - 边界检查：落在填充区域输出灰色(114)
 *    - 最近邻采样：直接取整（速度快，YOLO训练时也这样）
 * 
 * 4. 常见填充值：
 *    - OpenCV默认：114 (0x72, 中性灰)
 *    - ImageNet均值：[104,117,123] (BGR顺序)
 */

#include "letterbox.cuh"
#include <cuda_runtime.h>
#include <algorithm>
#include <cstdint>

/*
 * letterbox_kernel：CUDA核函数，每个线程处理目标图像的一个像素
 * 
 * 反向映射公式：
 *   sx = (x - offx) / scale
 *   sy = (y - offy) / scale
 * 
 * 其中：
 *   (x,y)：目标图像坐标
 *   (sx,sy)：源图像坐标
 *   scale：缩放比例
 *   (offx,offy)：填充偏移
 */
static __global__ void letterbox_kernel(const uint8_t* src, int sw, int sh, uint8_t* dst, int dw, int dh, float scale, int offx, int offy) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= dw || y >= dh) return;

    int dst_idx = (y * dw + x) * 3;

    // 反向映射：从目标坐标计算源坐标
    int sx = (int)((x - offx) / scale);
    int sy = (int)((y - offy) / scale);
    
    // 边界检查：如果落在填充区域，输出灰色(114)
    if (sx < 0 || sx >= sw || sy < 0 || sy >= sh) {
        dst[dst_idx + 0] = 114; 
        dst[dst_idx + 1] = 114; 
        dst[dst_idx + 2] = 114;
        return;
    }
    
    // 正常区域：复制源像素（最近邻采样）
    int src_idx = (sy * sw + sx) * 3;
    dst[dst_idx + 0] = src[src_idx + 0];
    dst[dst_idx + 1] = src[src_idx + 1];
    dst[dst_idx + 2] = src[src_idx + 2];
}

/*
 * letterbox_cuda：主机端调用函数，计算缩放参数并启动核函数
 * 
 * 参数：
 *   d_src：源图像设备指针（RGB24）
 *   src_w,src_h：源图像尺寸
 *   d_dst：目标图像设备指针（RGB24）
 *   dst_w,dst_h：目标图像尺寸（网络输入尺寸）
 *   stream：CUDA流
 * 
 * 注意：所有指针都已在主机端分配好设备内存
 */
void letterbox_cuda(const uint8_t* d_src, int src_w, int src_h, uint8_t* d_dst, int dst_w, int dst_h, cudaStream_t stream) {
    // 2.1 计算宽高各自的缩放比例
    float r_w = (float)dst_w / src_w;
    float r_h = (float)dst_h / src_h;
    
    // 2.2 选择较小比例，保证完整显示
    float scale = (r_w < r_h) ? r_w : r_h;
    
    // 2.3 计算缩放后实际图像尺寸
    int new_w = (int)(src_w * scale);
    int new_h = (int)(src_h * scale);
    
    // 2.4 计算填充偏移量（居中显示）
    int offx = (dst_w - new_w) / 2;
    int offy = (dst_h - new_h) / 2;

    dim3 block(16, 16);
    dim3 grid((dst_w + block.x - 1) / block.x, (dst_h + block.y - 1) / block.y);
    
    // 在指定流上启动核函数
    letterbox_kernel<<<grid, block, 0, stream>>>(d_src, src_w, src_h, d_dst, dst_w, dst_h, scale, offx, offy);
}
```

# 注释版本（全部文件）

下面是每个示例的注释（详解）版本，放在简洁代码块之后，便于查阅每段代码的作用与参数说明。

## hw_decode_demo.cpp （注释版）

```cpp
// 注释：这是面向小白的逐段说明版，保留原始逻辑并在重要步骤添加解释。
#include <iostream>

// 包含 FFmpeg 的 C 头，需要 extern "C" 防止 C++ 名字修饰
extern "C" {
#include <libavformat/avformat.h>   // 媒体格式（容器）接口
#include <libavcodec/avcodec.h>     // 编解码器接口
#include <libavutil/avutil.h>       // 工具函数和基本类型
#include <libavutil/imgutils.h>     // 图像工具（内存大小计算）
#include <libavutil/hwcontext.h>    // 硬件解码/设备上下文（可选）
#include <libswscale/swscale.h>     // 像素格式转换（YUV->RGB）
}

// get_hw_format：当使用硬件解码时，FFmpeg 会询问可用的像素格式
// 这里演示简单返回第一个格式（真实项目中应检测支持的格式并选择合适的）
static enum AVPixelFormat get_hw_format(AVCodecContext *ctx, const enum AVPixelFormat *pix_fmts) {
    for (const enum AVPixelFormat *p = pix_fmts; *p != -1; p++) {
        return *p; // 返回第一个支持的格式
    }
    return AV_PIX_FMT_NONE;
}

// open_input：打开文件并创建解码器上下文
// 关键步骤：打开输入 -> 读取流信息 -> 找到视频流 -> 创建并打开解码器
bool open_input(const char* filename, AVFormatContext** outFmtCtx, AVCodecContext** outDecCtx, int* outVideoStream) {
    av_log_set_level(AV_LOG_ERROR); // 减少日志噪声
    AVFormatContext* fmt = nullptr;
    // 打开媒体容器（例如 mp4/mkv）
    if (avformat_open_input(&fmt, filename, nullptr, nullptr) < 0) {
        std::cerr << "Failed to open input: " << filename << "\n";
        return false;
    }
    // 读取流信息（解码需要了解流的参数）
    if (avformat_find_stream_info(fmt, nullptr) < 0) {
        std::cerr << "Failed to find stream info\n";
        avformat_close_input(&fmt);
        return false;
    }
    // 查找第一个视频流的索引
    int video_stream = -1;
    for (unsigned i = 0; i < fmt->nb_streams; ++i) {
        if (fmt->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) { video_stream = i; break; }
    }
    if (video_stream < 0) { std::cerr << "No video stream found\n"; avformat_close_input(&fmt); return false; }

    // 根据流参数查找合适的解码器并创建解码器上下文
    AVCodecParameters* par = fmt->streams[video_stream]->codecpar;
    const AVCodec* decoder = avcodec_find_decoder(par->codec_id);
    if (!decoder) { std::cerr << "Decoder not found\n"; avformat_close_input(&fmt); return false; }

    AVCodecContext* dec_ctx = avcodec_alloc_context3(decoder);
    if (!dec_ctx) { std::cerr << "Failed to alloc codec context\n"; avformat_close_input(&fmt); return false; }
    // 将流参数复制到解码上下文
    if (avcodec_parameters_to_context(dec_ctx, par) < 0) { std::cerr << "Failed to copy codec params\n"; avcodec_free_context(&dec_ctx); avformat_close_input(&fmt); return false; }

    // 如果要尝试硬件解码（CUDA），可以在这里创建并关联 hw device：
    // AVBufferRef* hw_device_ctx = nullptr;
    // av_hwdevice_ctx_create(&hw_device_ctx, AV_HWDEVICE_TYPE_CUDA, nullptr, nullptr, 0);
    // dec_ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);
    // dec_ctx->get_format = get_hw_format;
    // 注意：并非所有 FFmpeg 都编译了 CUDA/VAAPI 支持。

    // 打开解码器（准备接收 packet 并输出 frame）
    if (avcodec_open2(dec_ctx, decoder, nullptr) < 0) { std::cerr << "Failed to open codec\n"; avcodec_free_context(&dec_ctx); avformat_close_input(&fmt); return false; }

    *outFmtCtx = fmt; *outDecCtx = dec_ctx; *outVideoStream = video_stream;
    return true;
}

// decode_loop：读取包并解码为帧，示例会打印每帧的 PTS（时间戳）与像素格式
void decode_loop(AVFormatContext* fmtCtx, AVCodecContext* decCtx, int video_stream_index, int max_frames) {
    AVPacket* pkt = av_packet_alloc();   // 存放容器读取到的压缩包
    AVFrame* frame = av_frame_alloc();   // 存放解码得到的帧（可能是软件或硬件帧）
    AVFrame* sw_frame = av_frame_alloc();
    int count = 0;

    // 循环读取包并送到解码器
    while (av_read_frame(fmtCtx, pkt) >= 0 && count < max_frames) {
        if (pkt->stream_index == video_stream_index) {
            // 送包到解码器（非阻塞）
            if (avcodec_send_packet(decCtx, pkt) == 0) {
                // 从解码器接收帧，可能接收多帧（B帧等）
                while (avcodec_receive_frame(decCtx, frame) == 0) {
                    // frame->format 表示像素格式（如 YUV420P 或 AV_PIX_FMT_CUDA）
                    if (frame->format == AV_PIX_FMT_NONE) {
                        std::cout << "Received a hardware frame (unsupported format id).\n";
                    }
                    AVRational tb = fmtCtx->streams[video_stream_index]->time_base;
                    double pts_seconds = frame->best_effort_timestamp == AV_NOPTS_VALUE ? 0 : frame->best_effort_timestamp * av_q2d(tb);
                    std::cout << "Frame " << count << " pts=" << frame->best_effort_timestamp << " (" << pts_seconds << "s) format=" << frame->format << "\n";
                    ++count;
                    if (count >= max_frames) break;
                }
            }
        }
        av_packet_unref(pkt); // 释放包数据以便下一次读取
    }

    // 发送空包以 flush 解码器内部缓冲并接收剩余帧
    avcodec_send_packet(decCtx, nullptr);
    while (avcodec_receive_frame(decCtx, frame) == 0) {
        std::cout << "Flushed frame format=" << frame->format << "\n";
    }

    av_frame_free(&frame);
    av_frame_free(&sw_frame);
    av_packet_free(&pkt);
}

int main(int argc, char** argv) {
    if (argc < 2) { std::cout << "Usage: hw_decode_demo <video>\n"; return 0; }
    AVFormatContext* fmt = nullptr; AVCodecContext* dec = nullptr; int vid = -1;
    // open_input 会打开文件并准备解码器
    if (!open_input(argv[1], &fmt, &dec, &vid)) return 1;
    std::cout << "Opened " << argv[1] << ", decoding 10 frames...\n";
    decode_loop(fmt, dec, vid, 10);
    avcodec_free_context(&dec);
    avformat_close_input(&fmt);
    return 0;
}
```

## seek_demo.cpp （注释版）

```cpp
// 注释：seek_demo 的注释版本，解释时间基转换与 av_seek_frame 的用法
#include <iostream>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

// 将毫秒时间转换到流的时间基后调用 av_seek_frame 完成定位
bool seek_to_timestamp(AVFormatContext* fmtCtx, int stream_index, int64_t timestamp_ms) {
    if (!fmtCtx || stream_index < 0) return false;
    AVStream* st = fmtCtx->streams[stream_index];
    // av_rescale_q 用于在不同时间基之间换算数值：这里把 ms(1/1000) -> stream->time_base
    int64_t ts = av_rescale_q(timestamp_ms, (AVRational){1,1000}, st->time_base);
    // AVSEEK_FLAG_BACKWARD: 如果找不到精确帧则向前定位到最近的关键帧
    if (av_seek_frame(fmtCtx, stream_index, ts, AVSEEK_FLAG_BACKWARD) < 0) {
        std::cerr << "seek failed\n";
        return false;
    }
    // 定位后需刷新解码器内部缓冲（若后续继续解码）
    avcodec_flush_buffers(fmtCtx->streams[stream_index]->codec);
    return true;
}

int main(int argc, char** argv) {
    if (argc < 3) { std::cout << "Usage: seek_demo <video> <seek_ms>\n"; return 0; }
    const char* fn = argv[1];
    int64_t seek_ms = atoll(argv[2]);
    AVFormatContext* fmt = nullptr;
    if (avformat_open_input(&fmt, fn, nullptr, nullptr) < 0) { std::cerr << "open failed\n"; return 1; }
    if (avformat_find_stream_info(fmt, nullptr) < 0) { std::cerr << "find stream info failed\n"; avformat_close_input(&fmt); return 1; }
    int vindex = -1;
    for (unsigned i=0;i<fmt->nb_streams;i++) if (fmt->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) { vindex = i; break; }
    if (vindex < 0) { std::cerr << "no video stream\n"; avformat_close_input(&fmt); return 1; }

    if (!seek_to_timestamp(fmt, vindex, seek_ms)) { std::cerr << "seek failed\n"; avformat_close_input(&fmt); return 1; }
    std::cout << "seeked to " << seek_ms << " ms (stream " << vindex << ")\n";
    avformat_close_input(&fmt);
    return 0;
}
```

## speed_demo.cpp （注释版）

```cpp
// 注释：倍速播放控制的简易实现说明（不包含解码，仅展示时间控制逻辑）
#include <iostream>
#include <thread>
#include <chrono>

class PlaybackController {
public:
    PlaybackController(): speed_(1.0), paused_(false) {}
    void set_speed(double s) { speed_ = s; }
    double speed() const { return speed_; }
    void play() { paused_ = false; }
    void pause() { paused_ = true; }
    bool paused() const { return paused_; }
private:
    double speed_; // 当前倍速（例如 2.0 表示 2x）
    bool paused_;
};

int main() {
    PlaybackController ctrl;
    double frame_rate = 25.0; // 假设源视频是 25 FPS
    // 真实项目中 frame_rate 应由解码器或流信息提供
    double frame_interval_ms = 1000.0 / frame_rate;
    int frame = 0;
    ctrl.set_speed(2.0); // 设置为 2x

    for (;;) {
        if (!ctrl.paused()) {
            // 在 GUI 程序中这里会把解码后的一帧渲染到窗口
            std::cout << "Displaying frame " << frame << " at speed=" << ctrl.speed() << "x\n";
            ++frame;
        }
        // 按倍速调整等待时间：倍速越高两帧之间等待越短
        double wait_ms = frame_interval_ms / ctrl.speed();
        std::this_thread::sleep_for(std::chrono::milliseconds((int)wait_ms));
        if (frame >= 100) break; // 示例限制帧数
    }
    return 0;
}
```

## nv12_to_rgb.cuh （注释版）

```cpp
// 注释：头文件中的函数声明说明
// d_y/d_uv/d_rgb 都是 GPU 设备内存指针（设备指针），函数在 GPU 上执行色彩空间转换
// stream 可传 0 表示默认流，也可传自定义 cudaStream_t 做并行控制
void nv12_to_rgb_cuda(const uint8_t* d_y, const uint8_t* d_uv, int width, int height, uint8_t* d_rgb, cudaStream_t stream);
```

## nv12_to_rgb.cu （注释版）

```cpp
// 注释：关键点说明 — NV12 帧由 Y 平面和交错的 UV 平面组成
#include "nv12_to_rgb.cuh"
#include <cuda_runtime.h>
#include <cstdint>
#include <cstdio>

// kernel: 每个线程负责一个像素位置 (x,y)
static __global__ void nv12_to_rgb_kernel(const uint8_t* y, const uint8_t* uv, int w, int h, uint8_t* rgb) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y_idx = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= w || y_idx >= h) return;

    // Y 平面按像素存储，UV 平面为每 2x2 像素共用一组 U/V
    int y_offset = y_idx * w + x;
    uint8_t Y = y[y_offset];

    int uv_x = x / 2;
    int uv_y = y_idx / 2;
    // UV 行宽为 w/2，每个像素对占 2 字节（U,V）
    int uv_offset = uv_y * (w / 2) * 2 + uv_x * 2;
    uint8_t U = uv[uv_offset + 0];
    uint8_t V = uv[uv_offset + 1];

    // 整数近似的 YUV->RGB 变换（常用 BT.601 近似）
    int C = (int)Y - 16;
    int D = (int)U - 128;
    int E = (int)V - 128;
    int R = (298 * C + 409 * E + 128) >> 8;
    int G = (298 * C - 100 * D - 208 * E + 128) >> 8;
    int B = (298 * C + 516 * D + 128) >> 8;

    // 裁剪到 [0,255]
    R = min(max(R, 0), 255);
    G = min(max(G, 0), 255);
    B = min(max(B, 0), 255);

    int out_offset = (y_idx * w + x) * 3;
    rgb[out_offset + 0] = (uint8_t)R;
    rgb[out_offset + 1] = (uint8_t)G;
    rgb[out_offset + 2] = (uint8_t)B;
}

// host wrapper：计算 grid/block 并在指定流上启动 kernel
void nv12_to_rgb_cuda(const uint8_t* d_y, const uint8_t* d_uv, int width, int height, uint8_t* d_rgb, cudaStream_t stream) {
    dim3 block(16, 16);
    dim3 grid((width + block.x - 1) / block.x, (height + block.y - 1) / block.y);
    // kernel<<<grid,block,0,stream>>> 在指定 stream 上执行，实现与其它 CUDA 操作并行
    nv12_to_rgb_kernel<<<grid, block, 0, stream>>>(d_y, d_uv, width, height, d_rgb);
}
```

## letterbox.cuh （注释版）

```cpp
// 注释：letterbox 的声明，解释参数含义
// d_src: 源图像（RGB24）设备指针
// src_w, src_h: 源宽高
// d_dst: 输出图像（RGB24）设备指针，大小为 dst_w*dst_h*3
// dst_w, dst_h: 输出宽高（网络输入尺寸）
// stream: CUDA 流
void letterbox_cuda(const uint8_t* d_src, int src_w, int src_h, uint8_t* d_dst, int dst_w, int dst_h, cudaStream_t stream);
```

## letterbox.cu （注释版）

```cpp
// 注释：letterbox 实现的要点说明
#include "letterbox.cuh"
#include <cuda_runtime.h>
#include <algorithm>
#include <cstdint>

// kernel 每个线程负责目标图像中的一个像素位置，计算对应的源像素（最近邻采样）
static __global__ void letterbox_kernel(const uint8_t* src, int sw, int sh, uint8_t* dst, int dw, int dh, float scale, int offx, int offy) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= dw || y >= dh) return;

    int dst_idx = (y * dw + x) * 3;

    // 反向映射：从目标坐标得到源坐标
    int sx = (int)((x - offx) / scale);
    int sy = (int)((y - offy) / scale);
    if (sx < 0 || sx >= sw || sy < 0 || sy >= sh) {
        // 在填充区域写入灰色(114)——常见于 YOLO 的预处理
        dst[dst_idx + 0] = 114; dst[dst_idx + 1] = 114; dst[dst_idx + 2] = 114;
        return;
    }
    int src_idx = (sy * sw + sx) * 3;
    dst[dst_idx + 0] = src[src_idx + 0];
    dst[dst_idx + 1] = src[src_idx + 1];
    dst[dst_idx + 2] = src[src_idx + 2];
}

void letterbox_cuda(const uint8_t* d_src, int src_w, int src_h, uint8_t* d_dst, int dst_w, int dst_h, cudaStream_t stream) {
    // 计算保持长宽比的缩放比例和偏移
    float r_w = (float)dst_w / src_w;
    float r_h = (float)dst_h / src_h;
    float scale = (r_w < r_h) ? r_w : r_h;
    int new_w = (int)(src_w * scale);
    int new_h = (int)(src_h * scale);
    int offx = (dst_w - new_w) / 2;
    int offy = (dst_h - new_h) / 2;

    dim3 block(16, 16);
    dim3 grid((dst_w + block.x - 1) / block.x, (dst_h + block.y - 1) / block.y);
    // 在 stream 上启动 kernel
    letterbox_kernel<<<grid, block, 0, stream>>>(d_src, src_w, src_h, d_dst, dst_w, dst_h, scale, offx, offy);
}
```

## nv12_demo.cpp （注释版）

```cpp
// 注释：主机端示例，演示如何在 host 分配设备内存、拷贝数据、调用 nv12_to_rgb
#include <iostream>
#include <cuda_runtime.h>
#include "nv12_to_rgb.cuh"

int main() {
    const int W = 640, H = 360;
    size_t y_size = W * H;
    size_t uv_size = (W/2) * (H/2) * 2; // NV12 UV 大小
    size_t rgb_size = W * H * 3;

    // host dummy 数据，仅为测试用
    uint8_t* h_y = (uint8_t*)malloc(y_size);
    uint8_t* h_uv = (uint8_t*)malloc(uv_size);
    for (size_t i=0;i<y_size;i++) h_y[i] = 128;
    for (size_t i=0;i<uv_size;i++) h_uv[i] = 128;

    // 在设备上分配内存并拷贝
    uint8_t *d_y=nullptr, *d_uv=nullptr, *d_rgb=nullptr;
    cudaMalloc(&d_y, y_size);
    cudaMalloc(&d_uv, uv_size);
    cudaMalloc(&d_rgb, rgb_size);

    cudaMemcpy(d_y, h_y, y_size, cudaMemcpyHostToDevice);
    cudaMemcpy(d_uv, h_uv, uv_size, cudaMemcpyHostToDevice);

    // 调用设备转换函数（stream 传 0 表示默认 stream）
    nv12_to_rgb_cuda(d_y, d_uv, W, H, d_rgb, 0);

    // 将结果拷回 host 检查
    uint8_t* h_rgb = (uint8_t*)malloc(rgb_size);
    cudaMemcpy(h_rgb, d_rgb, rgb_size, cudaMemcpyDeviceToHost);

    std::cout << "nv12_demo: first pixel RGB = " << (int)h_rgb[0] << "," << (int)h_rgb[1] << "," << (int)h_rgb[2] << "\n";

    free(h_y); free(h_uv); free(h_rgb);
    cudaFree(d_y); cudaFree(d_uv); cudaFree(d_rgb);
    return 0;
}
```

## letterbox_demo.cpp （注释版）

```cpp
// 注释：主机端调用 letterbox 的示例，演示如何准备数据并读取结果
#include <iostream>
#include <cuda_runtime.h>
#include "letterbox.cuh"

int main() {
    const int SW = 320, SH = 180;
    const int DW = 640, DH = 640;
    size_t src_size = SW * SH * 3;
    size_t dst_size = DW * DH * 3;

    uint8_t* h_src = (uint8_t*)malloc(src_size);
    for (size_t i=0;i<src_size;i++) h_src[i] = (uint8_t)(i & 255); // 填充测试图

    uint8_t *d_src=nullptr, *d_dst=nullptr;
    cudaMalloc(&d_src, src_size);
    cudaMalloc(&d_dst, dst_size);
    cudaMemcpy(d_src, h_src, src_size, cudaMemcpyHostToDevice);

    // 调用 letterbox，结果写入 d_dst
    letterbox_cuda(d_src, SW, SH, d_dst, DW, DH, 0);

    uint8_t* h_dst = (uint8_t*)malloc(dst_size);
    cudaMemcpy(h_dst, d_dst, dst_size, cudaMemcpyDeviceToHost);
    std::cout << "letterbox_demo: sample dst pixel = " << (int)h_dst[0] << "," << (int)h_dst[1] << "," << (int)h_dst[2] << "\n";

    free(h_src); free(h_dst);
    cudaFree(d_src); cudaFree(d_dst);
    return 0;
}
```

## examples/README.md

````

# examples 原型说明（构建与 API 说明）

目录包含以下最小原型：

- `hw_decode_demo.cpp`：使用 FFmpeg 打开视频并演示解码循环（包含硬件设备创建注释）。
- `seek_demo.cpp`：演示如何按毫秒时间戳对视频流进行 seek（使用 `av_seek_frame`）。
- `speed_demo.cpp`：播放控制逻辑示例（以帧间隔控制倍速与暂停）。
- `cuda/`：包含多文件 `.cu` / `.cuh` 原型：`nv12_to_rgb` 与 `letterbox`，并包含 host-side demo。

构建前准备：

- 安装系统依赖（Debian/Ubuntu 示例）：

```bash
sudo apt update
sudo apt install -y build-essential cmake pkg-config libavformat-dev libavcodec-dev libavutil-dev libswscale-dev nvidia-cuda-toolkit
```

- 如果使用自定义 FFmpeg 或 CUDA，请确保 `pkg-config` 能找到 FFmpeg，且 `nvcc`/CUDA 工具链在 PATH 中。

构建示例：

```bash
mkdir -p examples/build && cd examples/build
cmake ..
cmake --build . -j
```

运行（示例）：

- 解码示例（打印前 10 帧的信息）：
```bash
./hw_decode_demo /path/to/video.mp4
```
- seek 示例：
```bash
./seek_demo /path/to/video.mp4 5000   # 跳转到 5000 ms
```
- 倍速示例（控制台演示）：
```bash
./speed_demo
```
- CUDA 示例（NV12->RGB）：
```bash
./nv12_demo
```
- CUDA 示例（letterbox）：
```bash
./letterbox_demo
```

示例中关键 API 与作用（简明，面向初学者）：

- `open_input(const char* filename, AVFormatContext** outFmtCtx, AVCodecContext** outDecCtx, int* outVideoStream)` (在 `hw_decode_demo.cpp`)：
	- 作用：打开媒体文件，查找流信息，创建并打开视频解码器上下文。
	- 参数：
		- `filename`：视频文件路径。
		- `outFmtCtx`：输出的 `AVFormatContext*` 指针，后续用于读取数据与查询流信息。
		- `outDecCtx`：输出的 `AVCodecContext*`，用于解码视频包为帧。
		- `outVideoStream`：输出的视频流索引（在 `AVFormatContext->streams` 中的下标）。

- `decode_loop(AVFormatContext* fmtCtx, AVCodecContext* decCtx, int video_stream_index, int max_frames)` (在 `hw_decode_demo.cpp`)：
	- 作用：从 `fmtCtx` 读取包并发送给 `decCtx` 解码，逐帧接收并打印帧信息（PTS、格式等）。用于演示解码流程与帧读取。
	- 参数：
		- `fmtCtx`：打开的格式上下文。
		- `decCtx`：已打开的解码器上下文。
		- `video_stream_index`：要解码的视频流索引。
		- `max_frames`：处理的最大帧数（示例中用于控制输出量）。

- `seek_to_timestamp(AVFormatContext* fmtCtx, int stream_index, int64_t timestamp_ms)` (在 `seek_demo.cpp`)：
	- 作用：将媒体流定位到接近指定的时间戳，常用于实现拖动进度条或跳转播放。
	- 参数：
		- `fmtCtx`：打开的格式上下文。
		- `stream_index`：目标视频流索引。
		- `timestamp_ms`：目标时间（毫秒）。函数内部将毫秒转换为流的时间基并调用 `av_seek_frame`。

- `PlaybackController` 类（在 `speed_demo.cpp`）:
	- 作用：封装播放控制状态（当前倍速、暂停/播放），示例通过睡眠控制帧显示速率来模拟倍速播放。
	- 常用方法：`set_speed(double)`：设置倍速；`play()` / `pause()`：控制暂停状态；`speed()` / `paused()`：查询状态。

- `nv12_to_rgb_cuda(const uint8_t* d_y, const uint8_t* d_uv, int width, int height, uint8_t* d_rgb, cudaStream_t stream)` (在 `examples/cuda/nv12_to_rgb.cuh`)：
	- 作用：在设备（GPU）上将 NV12 格式（Y + interleaved UV）转换为 RGB24，输入/输出均为设备指针，避免主机-设备多次拷贝。
	- 参数：
		- `d_y`：指向设备上 Y 平面的指针。
		- `d_uv`：指向设备上交错的 UV 平面的指针。
		- `width`, `height`：源图像宽高。
		- `d_rgb`：输出的设备指针，存放 RGB24 数据，大小为 `width*height*3`。
		- `stream`：可选的 CUDA 流（示例中也可传 0），用于异步执行。

- `letterbox_cuda(const uint8_t* d_src, int src_w, int src_h, uint8_t* d_dst, int dst_w, int dst_h, cudaStream_t stream)` (在 `examples/cuda/letterbox.cuh`)：
	- 作用：在 GPU 上对 RGB 图像进行按比例缩放并在目标尺寸内居中填充（letterbox），常用于将任意分辨率帧调整为神经网络输入尺寸同时保持长宽比。
	- 参数：
		- `d_src`：源图像设备指针（RGB24）。
		- `src_w`, `src_h`：源宽高。
		- `d_dst`：目标设备指针，大小为 `dst_w*dst_h*3`。
		- `dst_w`, `dst_h`：目标宽高（网络输入大小）。
		- `stream`：CUDA 流，用于与其它 GPU 操作并行。

后续我可以：

- 在每个示例源文件中插入更详尽的注释（逐个 API 参数说明与在整套项目中的调用位置/作用），并在需要时把 `hw_decode_demo` 扩展为能够将 FFmpeg 解码得到的 NV12 帧直接传入 GPU kernel（此步依赖本地 FFmpeg 构建是否支持 GPU 显存导出）。
- 或者现在直接为每个函数生成小白友好的逐行注释版本（告诉我你的偏好）。
````

---

## hw_decode_demo.cpp

```cpp
#include <iostream>

// 这个示例演示如何使用 FFmpeg 打开视频、初始化解码器，并进行逐帧解码。
// - 优先使用软件解码作为稳定回退。
// - 演示如何创建 HW device（CUDA）并在需要时挂载到解码器（如系统支持时）。
// - 请注意：不同平台/FFmpeg 编译方式会影响是否能直接获得 CUDA 设备指针；
//   生产代码中通常需要结合 FFmpeg 的 hwframe 特性或 NVIDIA Video Codec SDK。

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libavutil/hwcontext.h>
#include <libswscale/swscale.h>
}

static enum AVPixelFormat get_hw_format(AVCodecContext *ctx, const enum AVPixelFormat *pix_fmts) {
    for (const enum AVPixelFormat *p = pix_fmts; *p != -1; p++) {
        return *p;
    }
    return AV_PIX_FMT_NONE;
}

bool open_input(const char* filename, AVFormatContext** outFmtCtx, AVCodecContext** outDecCtx, int* outVideoStream) {
    av_log_set_level(AV_LOG_ERROR);
    AVFormatContext* fmt = nullptr;
    if (avformat_open_input(&fmt, filename, nullptr, nullptr) < 0) {
        std::cerr << "Failed to open input: " << filename << "\n";
        return false;
    }
    if (avformat_find_stream_info(fmt, nullptr) < 0) {
        std::cerr << "Failed to find stream info\n";
        avformat_close_input(&fmt);
        return false;
    }
    int video_stream = -1;
    for (unsigned i = 0; i < fmt->nb_streams; ++i) {
        if (fmt->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) { video_stream = i; break; }
    }
    if (video_stream < 0) { std::cerr << "No video stream found\n"; avformat_close_input(&fmt); return false; }

    AVCodecParameters* par = fmt->streams[video_stream]->codecpar;
    const AVCodec* decoder = avcodec_find_decoder(par->codec_id);
    if (!decoder) { std::cerr << "Decoder not found\n"; avformat_close_input(&fmt); return false; }

    AVCodecContext* dec_ctx = avcodec_alloc_context3(decoder);
    if (!dec_ctx) { std::cerr << "Failed to alloc codec context\n"; avformat_close_input(&fmt); return false; }
    if (avcodec_parameters_to_context(dec_ctx, par) < 0) { std::cerr << "Failed to copy codec params\n"; avcodec_free_context(&dec_ctx); avformat_close_input(&fmt); return false; }

    if (avcodec_open2(dec_ctx, decoder, nullptr) < 0) { std::cerr << "Failed to open codec\n"; avcodec_free_context(&dec_ctx); avformat_close_input(&fmt); return false; }

    *outFmtCtx = fmt; *outDecCtx = dec_ctx; *outVideoStream = video_stream;
    return true;
}

void decode_loop(AVFormatContext* fmtCtx, AVCodecContext* decCtx, int video_stream_index, int max_frames) {
    AVPacket* pkt = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    AVFrame* sw_frame = av_frame_alloc();
    int got = 0; int count = 0;

    while (av_read_frame(fmtCtx, pkt) >= 0 && count < max_frames) {
        if (pkt->stream_index == video_stream_index) {
            if (avcodec_send_packet(decCtx, pkt) == 0) {
                while (avcodec_receive_frame(decCtx, frame) == 0) {
                    if (frame->format == AV_PIX_FMT_NONE) {
                        std::cout << "Received a hardware frame (unsupported format id).\n";
                    }
                    AVRational tb = fmtCtx->streams[video_stream_index]->time_base;
                    double pts_seconds = frame->best_effort_timestamp == AV_NOPTS_VALUE ? 0 : frame->best_effort_timestamp * av_q2d(tb);
                    std::cout << "Frame " << count << " pts=" << frame->best_effort_timestamp << " (" << pts_seconds << "s) format=" << frame->format << "\n";
                    ++count;
                    if (count >= max_frames) break;
                }
            }
        }
        av_packet_unref(pkt);
    }

    avcodec_send_packet(decCtx, nullptr);
    while (avcodec_receive_frame(decCtx, frame) == 0) {
        std::cout << "Flushed frame format=" << frame->format << "\n";
    }

    av_frame_free(&frame);
    av_frame_free(&sw_frame);
    av_packet_free(&pkt);
}

int main(int argc, char** argv) {
    if (argc < 2) { std::cout << "Usage: hw_decode_demo <video>\n"; return 0; }
    AVFormatContext* fmt = nullptr; AVCodecContext* dec = nullptr; int vid = -1;
    if (!open_input(argv[1], &fmt, &dec, &vid)) return 1;
    std::cout << "Opened " << argv[1] << ", decoding 10 frames...\n";
    decode_loop(fmt, dec, vid, 10);
    avcodec_free_context(&dec);
    avformat_close_input(&fmt);
    return 0;
}

```

---

## seek_demo.cpp

```cpp
#include <iostream>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

bool seek_to_timestamp(AVFormatContext* fmtCtx, int stream_index, int64_t timestamp_ms) {
    if (!fmtCtx || stream_index < 0) return false;
    AVStream* st = fmtCtx->streams[stream_index];
    int64_t ts = av_rescale_q(timestamp_ms, (AVRational){1,1000}, st->time_base);
    if (av_seek_frame(fmtCtx, stream_index, ts, AVSEEK_FLAG_BACKWARD) < 0) {
        std::cerr << "seek failed\n";
        return false;
    }
    avcodec_flush_buffers(fmtCtx->streams[stream_index]->codec);
    return true;
}

int main(int argc, char** argv) {
    if (argc < 3) { std::cout << "Usage: seek_demo <video> <seek_ms>\n"; return 0; }
    const char* fn = argv[1];
    int64_t seek_ms = atoll(argv[2]);
    AVFormatContext* fmt = nullptr;
    if (avformat_open_input(&fmt, fn, nullptr, nullptr) < 0) { std::cerr << "open failed\n"; return 1; }
    if (avformat_find_stream_info(fmt, nullptr) < 0) { std::cerr << "find stream info failed\n"; avformat_close_input(&fmt); return 1; }
    int vindex = -1;
    for (unsigned i=0;i<fmt->nb_streams;i++) if (fmt->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) { vindex = i; break; }
    if (vindex < 0) { std::cerr << "no video stream\n"; avformat_close_input(&fmt); return 1; }

    if (!seek_to_timestamp(fmt, vindex, seek_ms)) { std::cerr << "seek failed\n"; avformat_close_input(&fmt); return 1; }
    std::cout << "seeked to " << seek_ms << " ms (stream " << vindex << ")\n";
    avformat_close_input(&fmt);
    return 0;
}

```

---

## speed_demo.cpp

```cpp
#include <iostream>
#include <thread>
#include <chrono>

class PlaybackController {
public:
    PlaybackController(): speed_(1.0), paused_(false) {}
    void set_speed(double s) { speed_ = s; }
    double speed() const { return speed_; }
    void play() { paused_ = false; }
    void pause() { paused_ = true; }
    bool paused() const { return paused_; }
private:
    double speed_;
    bool paused_;
};

int main() {
    PlaybackController ctrl;
    double frame_rate = 25.0;
    double frame_interval_ms = 1000.0 / frame_rate;
    int frame = 0;
    ctrl.set_speed(2.0);

    for (;;) {
        if (!ctrl.paused()) {
            std::cout << "Displaying frame " << frame << " at speed=" << ctrl.speed() << "x\n";
            ++frame;
        }
        double wait_ms = frame_interval_ms / ctrl.speed();
        std::this_thread::sleep_for(std::chrono::milliseconds((int)wait_ms));
        if (frame >= 100) break;
    }
    return 0;
}

```

---

## CUDA: nv12_to_rgb.cuh

```cpp
#pragma once
#include <cuda_runtime.h>
#include <cstdint>

void nv12_to_rgb_cuda(const uint8_t* d_y, const uint8_t* d_uv, int width, int height, uint8_t* d_rgb, cudaStream_t stream);

```

## CUDA: nv12_to_rgb.cu

```cpp
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

```

## CUDA: letterbox.cuh

```cpp
#pragma once
#include <cuda_runtime.h>
#include <cstdint>

void letterbox_cuda(const uint8_t* d_src, int src_w, int src_h, uint8_t* d_dst, int dst_w, int dst_h, cudaStream_t stream);

```

## CUDA: letterbox.cu

```cpp
#include "letterbox.cuh"
#include <cuda_runtime.h>
#include <algorithm>
#include <cstdint>

static __global__ void letterbox_kernel(const uint8_t* src, int sw, int sh, uint8_t* dst, int dw, int dh, float scale, int offx, int offy) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= dw || y >= dh) return;

    int dst_idx = (y * dw + x) * 3;

    int sx = (int)((x - offx) / scale);
    int sy = (int)((y - offy) / scale);
    if (sx < 0 || sx >= sw || sy < 0 || sy >= sh) {
        dst[dst_idx + 0] = 114; dst[dst_idx + 1] = 114; dst[dst_idx + 2] = 114;
        return;
    }
    int src_idx = (sy * sw + sx) * 3;
    dst[dst_idx + 0] = src[src_idx + 0];
    dst[dst_idx + 1] = src[src_idx + 1];
    dst[dst_idx + 2] = src[src_idx + 2];
}

void letterbox_cuda(const uint8_t* d_src, int src_w, int src_h, uint8_t* d_dst, int dst_w, int dst_h, cudaStream_t stream) {
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

```

## nv12_demo.cpp

```cpp
#include <iostream>
#include <cuda_runtime.h>
#include "nv12_to_rgb.cuh"

int main() {
    const int W = 640, H = 360;
    size_t y_size = W * H;
    size_t uv_size = (W/2) * (H/2) * 2;
    size_t rgb_size = W * H * 3;

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
```

## letterbox_demo.cpp

```cpp
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
```

