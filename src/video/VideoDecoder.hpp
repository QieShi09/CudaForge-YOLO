#ifndef VIDEO_DECODER_HPP
#define VIDEO_DECODER_HPP

#include <QObject>
#include <string>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <cuda_runtime_api.h>
#include "VideoDemuxer.hpp"
#include "PacketQueue.hpp"
#include "FrameQueue.hpp"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
}

class VideoDecoder : public QObject {
    Q_OBJECT
public:
    explicit VideoDecoder(const std::string& url, int channel_id, FrameQueue* frame_queue, QObject* parent = nullptr);
    ~VideoDecoder();

    // 禁止拷贝（多线程对象不应被拷贝）
    VideoDecoder(const VideoDecoder&) = delete;
    VideoDecoder& operator=(const VideoDecoder&) = delete;

public Q_SLOTS:
    // 启动解码的主循环（由 QThread 触发）
    void startDecoding();

    // 停止解码
    void stopDecoding();

    // 设置期望的显示尺寸（用于优化 GPU->CPU 传输带宽）
    // 在 3x3 模式下，传入小尺寸（如 640x360）；在 1x1 模式下，传入大尺寸
    void setDisplaySize(int width, int height);

    // 设置低帧率模式 (用于网格模式省资源)
    void setLowFPSMode(bool low) { low_fps_mode_ = low; }

    // 设置目标帧率 (带约束)
    // range: 5-60, step: 5
    void setTargetFPS(int fps);

    // 是否允许目标帧率超过原生帧率（压测用）
    void setAllowOverNativeFPS(bool allow);

    // 获取当前目标帧率
    int getTargetFPS() const { return target_fps_.load(); }

    // 获取视频原生帧率 (用于前端设置 Slider 最大值)
    double getNativeFPS();

    // 设置暂停状态
    // true: 暂停解码/显示（画面静止，但后台可能仍在接收流以防断连）
    // false: 恢复正常播放
    void setPaused(bool paused);

    // 设置播放倍速 (仅对文件有效)
    // speed: 0.5, 1.0, 2.0, 4.0 等
    void setSpeed(float speed);

    // 跳转到指定时间 (仅对文件有效)
    // timestamp_ms: 毫秒级时间戳
    void seek(int64_t timestamp_ms);

    // 获取视频总时长 (毫秒)，如果是流媒体则返回 0
    int64_t getDuration() const;

    // 获取当前播放位置 (毫秒)
    int64_t getCurrentPosition() const { return current_position_ms_.load(); }
    
    // 是否已播放结束
    bool isEOF() const { return is_eof_.load(); }
    
    // 是否为文件模式
    bool isFileMode() const { return is_file_mode_; }

    // 设置当前检测 Epoch (用于防止旧帧覆盖新结果)
    void setChannelEpoch(uint64_t epoch);

    // --- Display Pool Management for Zero-Middleware Malloc ---
    // Instead of malloc/free per frame, we use a fixed pool of display buffers
    // allocated ONCE at startup. If pool is empty, we drop display frames.
    // Detection path uses direct NVDEC pointers (zero copy, zero alloc).
    void initDisplayPool(size_t width, size_t height);
    void releaseDisplayPool();
    uint8_t* getDisplayBufferFromPool();
    void returnDisplayBufferToPool(uint8_t* ptr);

    // Static callback for AVBufferRef
    static void releaseDisplayBufferCallback(void* opaque, uint8_t* data);

    // 统计：解码器显存与数量（估算）
    static size_t totalDecoderVramBytes();
    static size_t totalStandaloneFrameVramBytes();
    static void registerStandaloneFrameAlloc(size_t bytes);
    static void registerStandaloneFrameFree(size_t bytes);
    static int hwDecoderCount();
    static int swDecoderCount();
    static int maxHwDecoders();

Q_SIGNALS:
    // 视频播放结束信号（仅文件模式）
    void playbackFinished(int channel_id);

private:
    // 全局显示缓冲池
    static std::mutex s_display_pool_mutex;
    static std::vector<uint8_t*> s_display_pool_pages;
    static std::vector<uint8_t*> s_display_pool_free;
    static size_t s_display_pool_frame_size;
    static bool s_display_pool_inited;
    

    bool processImageSource();
    bool enqueueDetectionTensorFromNV12Frame(const AVFrame* frame);
    bool enqueueDetectionTensorFromRGBA(const uint8_t* dev_rgba, int width, int height, int pitch);

    // 内部初始化函数：负责寻找并激活 CUDA 硬件
    bool initHardware();

    // 打开解码器（纯 HW 模式）
    bool openCodec();
    void fallbackToSoftware(const char* reason);
    void updateDecoderStatsOnClose();
    
    // FFmpeg 回调：用于选择像素格式 (强制 CUDA)
    static enum AVPixelFormat get_hw_format(AVCodecContext *ctx, const enum AVPixelFormat *pix_fmts);

private:
    std::string url_;               // 视频源地址（文件或RTSP）
    std::atomic<bool> is_running_;  // 线程安全的运行标志位
    std::atomic<bool> is_paused_{false}; // 暂停标志位
    std::atomic<int> display_w_ = {1920}; // 默认显示宽度
    std::atomic<int> display_h_ = {1080}; // 默认显示高度
    std::atomic<bool> low_fps_mode_{true}; // 默认开启低帧率模式
    std::atomic<int> target_fps_{30};      // 目标帧率
    std::atomic<bool> allow_over_native_fps_{false}; // 是否允许超过原生帧率
    
    std::atomic<float> playback_speed_{1.0f}; // 播放倍速
    std::atomic<int64_t> current_position_ms_{0}; // 当前播放位置 (毫秒)
    std::atomic<bool> is_eof_{false};  // EOF 标志，用于 seek 恢复
    std::mutex eof_mutex_;              // EOF 等待的互斥锁
    std::condition_variable eof_cv_;    // EOF 等待的条件变量
    std::mutex codec_mutex_;            // 保护 cdc_ctx_ 的并发访问
    bool is_file_mode_ = false;     // 标记是否为本地文件
    int channel_id_ = -1;           // 通道 ID
    bool is_image_mode_ = false;    // 图片模式标记
    std::atomic<uint64_t> channel_epoch_{0};    // 通道 epoch，防止旧回调覆盖新结果

    // FFmpeg 核心组件
    AVCodecContext* cdc_ctx_ = nullptr; // 解码器上下文：负责核心解码任务
    AVBufferRef* hw_device_ctx_ = nullptr; // 硬件设备上下文：代表 NVIDIA 显卡
    const AVCodec* codec_ = nullptr;
    AVCodecParameters* codecpar_ = nullptr;
    bool hw_decode_enabled_ = true;
    bool using_hw_decoder_ = false;
    bool decoder_mode_counted_ = false;
    size_t decoder_vram_bytes_ = 0;
    cudaStream_t det_upload_stream_ = nullptr;

    static std::atomic<size_t> s_total_decoder_vram_bytes;
    static std::atomic<size_t> s_total_standalone_frame_vram_bytes;
    static std::atomic<int> s_hw_decoder_count;
    static std::atomic<int> s_sw_decoder_count;

    // 核心组件
    PacketQueue queue_;
    FrameQueue* frame_queue_; // 输出缓冲池
    VideoDemuxer demuxer_;
};

#endif
