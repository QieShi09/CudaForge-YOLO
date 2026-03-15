#include "VideoDecoder.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <QImage>
#include <QDebug>
#include <cuda_runtime.h>
#include <cstdlib>
#include "../core/InputFrameArenaStore.hpp"
#include "../engine/TRTDetector.hpp"
#include "../kernels/CudaImageProc.cuh"
#include "../core/PipelineStats.hpp"
#include "../core/NvtxUtils.hpp"

extern "C" {
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

static void cuda_buf_free(void* opaque, uint8_t* data);

namespace {
struct CudaBufMeta {
    size_t bytes = 0;
};

AVFrame* make_cuda_rgba_frame(uint8_t* dev_rgba, int width, int height, size_t bytes) {
    if (!dev_rgba || width <= 0 || height <= 0 || bytes == 0) return nullptr;

    AVFrame* frame = av_frame_alloc();
    if (!frame) return nullptr;

    frame->format = AV_PIX_FMT_CUDA;
    frame->width = width;
    frame->height = height;

    CudaBufMeta* meta = new CudaBufMeta();
    meta->bytes = bytes;
    frame->buf[0] = av_buffer_create(dev_rgba, static_cast<int>(bytes), cuda_buf_free, meta, 0);
    if (!frame->buf[0]) {
        delete meta;
        av_frame_free(&frame);
        cudaFree(dev_rgba);
        return nullptr;
    }

    VideoDecoder::registerStandaloneFrameAlloc(bytes);
    frame->data[0] = dev_rgba;
    frame->data[1] = nullptr;
    frame->linesize[0] = width * 4;
    frame->linesize[1] = 0;
    return frame;
}
}

// CUDA 缓冲区释放回调，供 AVBufferRef 引用计数归零时调用
static void cuda_buf_free(void* opaque, uint8_t* data) {
    CudaBufMeta* meta = static_cast<CudaBufMeta*>(opaque);
    if (meta) {
        VideoDecoder::registerStandaloneFrameFree(meta->bytes);
        delete meta;
    }
    cudaFree(data);
}

std::atomic<size_t> VideoDecoder::s_total_decoder_vram_bytes{0};
std::atomic<size_t> VideoDecoder::s_total_standalone_frame_vram_bytes{0};
std::atomic<int> VideoDecoder::s_hw_decoder_count{0};
std::atomic<int> VideoDecoder::s_sw_decoder_count{0};

size_t VideoDecoder::totalDecoderVramBytes() {
    return s_total_decoder_vram_bytes.load(std::memory_order_relaxed);
}

size_t VideoDecoder::totalStandaloneFrameVramBytes() {
    return s_total_standalone_frame_vram_bytes.load(std::memory_order_relaxed);
}

void VideoDecoder::registerStandaloneFrameAlloc(size_t bytes) {
    s_total_standalone_frame_vram_bytes.fetch_add(bytes, std::memory_order_relaxed);
}

void VideoDecoder::registerStandaloneFrameFree(size_t bytes) {
    s_total_standalone_frame_vram_bytes.fetch_sub(bytes, std::memory_order_relaxed);
}

int VideoDecoder::hwDecoderCount() {
    return s_hw_decoder_count.load(std::memory_order_relaxed);
}

int VideoDecoder::swDecoderCount() {
    return s_sw_decoder_count.load(std::memory_order_relaxed);
}

int VideoDecoder::maxHwDecoders() {
    static int cached = -1;
    if (cached >= 0) return cached;
    const char* env = std::getenv("CUDAFORGE_MAX_HW_DECODERS");
    if (env) {
        int v = std::atoi(env);
        if (v >= 0) { cached = v; return cached; }
    }
    // Ada Lovelace (RTX 40xx) 驱动 525+ 已移除并发 NVDEC 会话数限制
    // 默认 16，可通过 CUDAFORGE_MAX_HW_DECODERS 覆盖（0 = 无限制）
    cached = 16;
    return cached;
}

bool VideoDecoder::enqueueDetectionTensorFromNV12Frame(const AVFrame* frame)
{
    if (!frame || !frame->data[0] || !frame->data[1]) return false;

    int model_w = TRTDetector::getInstance().getInputW();
    int model_h = TRTDetector::getInstance().getInputH();
    if (model_w <= 0 || model_h <= 0) return false;

    float r = std::min(static_cast<float>(model_w) / frame->width,
                       static_cast<float>(model_h) / frame->height);
    int new_w = static_cast<int>(frame->width * r);
    int new_h = static_cast<int>(frame->height * r);
    Slot::PreprocMeta meta;
    meta.orig_w = frame->width;
    meta.orig_h = frame->height;
    meta.scale = r;
    meta.pad_w = (model_w - new_w) / 2;
    meta.pad_h = (model_h - new_h) / 2;

    bool pushed = InputFrameArenaStore::getInstance().pushFrame(
        channel_id_, channel_epoch_,
        frame->pts > 0 ? static_cast<int64_t>(frame->pts) : 0,
        meta,
        det_upload_stream_ ? det_upload_stream_ : static_cast<cudaStream_t>(0),
        [frame, model_w, model_h](void* dst, size_t bytes, cudaStream_t stream) -> bool {
            size_t need = static_cast<size_t>(3) * model_w * model_h * sizeof(float);
            if (!dst || bytes < need) return false;
            launchNV12ToFloatNCHWDevice(
                reinterpret_cast<const uint8_t*>(frame->data[0]),
                reinterpret_cast<const uint8_t*>(frame->data[1]),
                frame->linesize[0],
                frame->linesize[1],
                frame->width,
                frame->height,
                static_cast<float*>(dst),
                model_w,
                model_h,
                stream);
            return true;
        });

    if (!pushed) {
        PipelineStats::getInstance().frames_dropped_dq.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    PipelineStats::getInstance().frames_pushed_dq.fetch_add(1, std::memory_order_relaxed);
    return true;
}

bool VideoDecoder::enqueueDetectionTensorFromRGBA(const uint8_t* dev_rgba, int width, int height, int pitch)
{
    if (!dev_rgba || width <= 0 || height <= 0 || pitch <= 0) return false;

    int model_w = TRTDetector::getInstance().getInputW();
    int model_h = TRTDetector::getInstance().getInputH();
    if (model_w <= 0 || model_h <= 0) return false;

    float r = std::min(static_cast<float>(model_w) / width,
                       static_cast<float>(model_h) / height);
    int new_w = static_cast<int>(width * r);
    int new_h = static_cast<int>(height * r);
    Slot::PreprocMeta meta;
    meta.orig_w = width;
    meta.orig_h = height;
    meta.scale = r;
    meta.pad_w = (model_w - new_w) / 2;
    meta.pad_h = (model_h - new_h) / 2;

    bool pushed = InputFrameArenaStore::getInstance().pushFrame(
        channel_id_, channel_epoch_, 0,
        meta,
        det_upload_stream_ ? det_upload_stream_ : static_cast<cudaStream_t>(0),
        [dev_rgba, pitch, width, height, model_w, model_h](void* dst, size_t bytes, cudaStream_t stream) -> bool {
            size_t need = static_cast<size_t>(3) * model_w * model_h * sizeof(float);
            if (!dst || bytes < need) return false;
            launchResizeLetterboxToFloatNCHW(
                dev_rgba,
                pitch,
                width,
                height,
                static_cast<float*>(dst),
                model_w,
                model_h,
                stream);
            return true;
        });

    if (!pushed) {
        PipelineStats::getInstance().frames_dropped_dq.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    PipelineStats::getInstance().frames_pushed_dq.fetch_add(1, std::memory_order_relaxed);
    return true;
}

bool VideoDecoder::processImageSource()
{
    QImage image(QString::fromStdString(url_));
    if (image.isNull()) {
        fprintf(stderr, "[VideoDecoder] ch=%d: failed to load image: %s\n", channel_id_, url_.c_str());
        return false;
    }

    QImage rgba = image.convertToFormat(QImage::Format_RGBA8888);
    if (rgba.isNull()) {
        fprintf(stderr, "[VideoDecoder] ch=%d: failed to convert image to RGBA: %s\n", channel_id_, url_.c_str());
        return false;
    }

    const int width = rgba.width();
    const int height = rgba.height();
    const int pitch = rgba.bytesPerLine();
    const size_t rgba_bytes = static_cast<size_t>(pitch) * height;

    uint8_t* det_rgba = nullptr;
    cudaError_t det_alloc = cudaMalloc(&det_rgba, rgba_bytes);
    if (det_alloc != cudaSuccess) {
        fprintf(stderr, "[VideoDecoder] ch=%d: cudaMalloc image detect buffer failed: %s\n",
                channel_id_, cudaGetErrorString(det_alloc));
        return false;
    }
    cudaError_t det_copy = cudaMemcpy2D(det_rgba, pitch,
                                        rgba.constBits(), pitch,
                                        pitch, height, cudaMemcpyHostToDevice);
    if (det_copy != cudaSuccess) {
        fprintf(stderr, "[VideoDecoder] ch=%d: image upload for detect failed: %s\n",
                channel_id_, cudaGetErrorString(det_copy));
        cudaFree(det_rgba);
        return false;
    }

    if (InputFrameArenaStore::getInstance().isChannelEnabled(channel_id_)) {
        enqueueDetectionTensorFromRGBA(det_rgba, width, height, pitch);
        if (det_upload_stream_) {
            cudaStreamSynchronize(det_upload_stream_);
        }
    }
    cudaFree(det_rgba);

    uint8_t* disp_rgba = nullptr;
    cudaError_t disp_alloc = cudaMalloc(&disp_rgba, rgba_bytes);
    if (disp_alloc != cudaSuccess) {
        fprintf(stderr, "[VideoDecoder] ch=%d: cudaMalloc image display buffer failed: %s\n",
                channel_id_, cudaGetErrorString(disp_alloc));
        return false;
    }
    cudaError_t disp_copy = cudaMemcpy2D(disp_rgba, pitch,
                                         rgba.constBits(), pitch,
                                         pitch, height, cudaMemcpyHostToDevice);
    if (disp_copy != cudaSuccess) {
        fprintf(stderr, "[VideoDecoder] ch=%d: image upload for display failed: %s\n",
                channel_id_, cudaGetErrorString(disp_copy));
        cudaFree(disp_rgba);
        return false;
    }

    AVFrame* image_frame = make_cuda_rgba_frame(disp_rgba, width, height, rgba_bytes);
    if (!image_frame) {
        return false;
    }
    image_frame->pts = 0;

    PipelineStats::getInstance().frames_decoded.fetch_add(1, std::memory_order_relaxed);
    bool fq_ok = frame_queue_ ? frame_queue_->pushDropOldest(image_frame) : false;
    if (!fq_ok) {
        av_frame_free(&image_frame);
        PipelineStats::getInstance().frames_dropped_fq.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    PipelineStats::getInstance().frames_pushed_fq.fetch_add(1, std::memory_order_relaxed);
    return true;
}

VideoDecoder::VideoDecoder(const std::string &url, int channel_id, FrameQueue *frame_queue, QObject *parent)
    : QObject(parent), url_(url), is_running_(false), channel_id_(channel_id), frame_queue_(frame_queue), demuxer_(&queue_)
{
    // 在构造时判断是否为文件模式，使 setPaused() 在 startDecoding() 之前调用也能正确工作
    is_file_mode_ = (url_.find("rtsp://") == std::string::npos &&
                     url_.find("rtmp://") == std::string::npos &&
                     url_.find("/dev/video") == std::string::npos);
    // 检测是否为图片文件（扩展名判断）
    std::string lower = url_;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    if (lower.find(".jpg") != std::string::npos || lower.find(".jpeg") != std::string::npos ||
        lower.find(".png") != std::string::npos || lower.find(".bmp") != std::string::npos ||
        lower.find(".tif") != std::string::npos || lower.find(".tiff") != std::string::npos) {
        is_image_mode_ = true;
    }
}

VideoDecoder::~VideoDecoder()
{
    // 确保解码循环已经停止
    stopDecoding();

    if (det_upload_stream_) {
        cudaStreamDestroy(det_upload_stream_);
        det_upload_stream_ = nullptr;
    }

    std::lock_guard<std::mutex> lk(codec_mutex_);

    // 释放顺序：解码器上下文 -> 硬件上下文
    if (cdc_ctx_)
    {
        avcodec_free_context(&cdc_ctx_);
        cdc_ctx_ = nullptr;
    }
    if (hw_device_ctx_)
    {
        av_buffer_unref(&hw_device_ctx_);
    }
}

void VideoDecoder::stopDecoding()
{
    is_running_ = false;

    // 停止包队列，唤醒可能卡在 queue_.push() 的 Demuxer 线程
    // 否则 demuxer_.stop() 中的 join() 会导致主线程死锁
    queue_.stop();

    // 停止拆包器，这也会唤醒可能卡在 queue.pop() 的解码循环
    demuxer_.stop();
    // 停止帧队列，唤醒可能卡在 frame_queue->push() 的解码循环
    if (frame_queue_)
        frame_queue_->stop();
    
    // 唤醒可能卡在 EOF 等待的解码线程
    is_eof_.store(false);
    eof_cv_.notify_one();
}

void VideoDecoder::setDisplaySize(int width, int height)
{
    if (width > 0 && height > 0)
    {
        display_w_ = width;
        display_h_ = height;
        // 可以在这里打印日志，验证 UI 切换是否生效
    }
}

void VideoDecoder::setPaused(bool paused)
{
    // 防止重复调用导致的冗余状态切换和 Decoder Flush
    // exchange 返回之前的值，如果相同说明状态未变，直接返回
    if (is_paused_.exchange(paused) == paused) {
        return;
    }

    // 区分文件模式和直播/摄像头模式
    if (is_file_mode_) {
        // 文件模式：暂停时清空队列，防止恢复后跳帧
        demuxer_.setPaused(paused);
        if (paused) {
            queue_.clear();
            if (frame_queue_) frame_queue_->clear();
        }
    } else {
        // 直播/摄像头模式：恢复时先清空队列，再恢复 demuxer
        if (std::lock_guard<std::mutex> lk(codec_mutex_);
            !paused) {
            // 先清空队列，丢弃缓存的旧帧
            queue_.clear();
            if (frame_queue_) frame_queue_->clear();
            // 清空解码器内部缓存
            if (cdc_ctx_) avcodec_flush_buffers(cdc_ctx_);
        }
        // 然后再控制 demuxer
        demuxer_.setPaused(paused);
    }
}

void VideoDecoder::setTargetFPS(int fps)
{
    // 使用视频原生帧率作为动态上限，而不是硬编码的 60
    double native = getNativeFPS();
    int upper_bound = std::max(5, static_cast<int>(std::ceil(native)));
    if (allow_over_native_fps_.load()) {
        upper_bound = std::max(upper_bound, fps);
    }
    target_fps_ = std::clamp(fps, 5, upper_bound);
}

void VideoDecoder::setAllowOverNativeFPS(bool allow)
{
    allow_over_native_fps_.store(allow, std::memory_order_relaxed);
    
    // allow=true 用于压测：允许 setTargetFPS 设置超过源帧率的值。
    // allow=false 常规行为：setTargetFPS 被限制在源帧率以内。
    // 注意：不再关闭 Demuxer 的节流，Demuxer 应始终根据 speed 进行节流，
    // 以防止文件读取速度完全失控导致队列溢出或 CPU 占满。
    // 如果需要极限性能测试，应通过 setSpeed 设置极大的倍速。
}

double VideoDecoder::getNativeFPS()
{
    if (!demuxer_.isOpen()) return 30.0; // 如果尚未打开，返回默认值
    return av_q2d(demuxer_.getFrameRate());
}

void VideoDecoder::setSpeed(float speed)
{
    if (is_file_mode_ && speed > 0.1f)
    {
        playback_speed_ = speed;
        // 同步设置 Demuxer 的读取速率
        demuxer_.setSpeed(speed);
    }
}

void VideoDecoder::setChannelEpoch(uint64_t epoch) {
    channel_epoch_.store(epoch, std::memory_order_relaxed);
}

void VideoDecoder::seek(int64_t timestamp_ms)
{
    if (!is_file_mode_)
        return;

    // 1. 重新启用帧队列（可能因 EOF 而停止）
    if (frame_queue_) {
        frame_queue_->start();
        frame_queue_->clear();
    }

    // 2. 调用 Demuxer 进行跳转（内部会处理 EOF 重启）
    demuxer_.seek(timestamp_ms);

    {
        std::lock_guard<std::mutex> lk(codec_mutex_);
        if (cdc_ctx_)
        {
            avcodec_flush_buffers(cdc_ctx_);
        }
    }
    
    // 4. 更新当前位置
    current_position_ms_.store(timestamp_ms, std::memory_order_relaxed);
    
    // 5. [关键] 如果处于 EOF 状态，唤醒解码线程继续工作
    if (is_eof_.load()) {
        is_eof_.store(false);
        eof_cv_.notify_one();
    }
}

int64_t VideoDecoder::getDuration() const
{
    // 代理给 demuxer
    return demuxer_.getDuration();
}

bool VideoDecoder::initHardware()
{
    // 每个解码器实例独立创建 FFmpeg CUDA ctx 包装
    // 原因：共享同一个 AVBufferRef 导致多线程并发 cuvidMapVideoFrame 时非法内存访问
    // 说明：底层 CUDA primary context 是 per-device 全局的（驱动层 cuDevicePrimaryCtxRetain），
    //       多次 av_hwdevice_ctx_create 不会各自占用 200MB，只是 FFmpeg 包装对象独立。
    int ret = av_hwdevice_ctx_create(&hw_device_ctx_, AV_HWDEVICE_TYPE_CUDA,
                                     nullptr, nullptr, 0);
    if (ret < 0) {
        char errbuf[256];
        av_strerror(ret, errbuf, sizeof(errbuf));
        fprintf(stderr, "[VideoDecoder] ch=%d: Cannot open CUDA hardware device: %s\n",
                channel_id_, errbuf);
        return false;
    }
    fprintf(stderr, "[VideoDecoder] ch=%d: CUDA hw context created (independent)\n", channel_id_);
    return true;
}

bool VideoDecoder::openCodec()
{
    if (!codecpar_ || !codec_) return false;

    // CAS 原子预留 NVDEC 槽位（纯 HW，无 SW fallback）
    bool hw_pre_reserved = false;
    {
        int max_hw = maxHwDecoders();
        while (true) {
            int current_hw = s_hw_decoder_count.load(std::memory_order_acquire);
            if (max_hw > 0 && current_hw >= max_hw) {
                fprintf(stderr, "[VideoDecoder] NVDEC limit reached (%d/%d), ch=%d cannot start.\n",
                        current_hw, max_hw, channel_id_);
                return false;
            }
            if (s_hw_decoder_count.compare_exchange_weak(current_hw, current_hw + 1,
                    std::memory_order_acq_rel, std::memory_order_relaxed)) {
                hw_pre_reserved = true;
                fprintf(stderr, "[VideoDecoder] HW slot reserved ch=%d (now %d)\n",
                        channel_id_, current_hw + 1);
                break;
            }
        }
    }

    fprintf(stderr, "[VideoDecoder] openCodec(ch=%d, HW)\n", channel_id_);
    fflush(stderr);

    std::lock_guard<std::mutex> lk(codec_mutex_);

    updateDecoderStatsOnClose();

    if (cdc_ctx_) {
        avcodec_free_context(&cdc_ctx_);
        cdc_ctx_ = nullptr;
    }

    cdc_ctx_ = avcodec_alloc_context3(codec_);
    if (!cdc_ctx_) {
        if (hw_pre_reserved) s_hw_decoder_count.fetch_sub(1, std::memory_order_relaxed);
        return false;
    }
    avcodec_parameters_to_context(cdc_ctx_, codecpar_);

    // 纯 HW：始终使用 NVDEC
    cdc_ctx_->get_format = get_hw_format;
    cdc_ctx_->hw_device_ctx = av_buffer_ref(hw_device_ctx_);
    int extra_hw_frames = 16;
    if (const char* env = std::getenv("CUDAFORGE_EXTRA_HW_FRAMES")) {
        int v = std::atoi(env);
        if (v > 0) extra_hw_frames = std::clamp(v, 4, 20);
    }
    cdc_ctx_->extra_hw_frames = extra_hw_frames;
    cdc_ctx_->thread_count = 1;
    cdc_ctx_->thread_type = 0;

    fprintf(stderr, "[VideoDecoder] avcodec_open2(ch=%d) calling...\n", channel_id_);
    int open_ret = avcodec_open2(cdc_ctx_, codec_, nullptr);
    if (open_ret < 0) {
        char errbuf[256];
        av_strerror(open_ret, errbuf, sizeof(errbuf));
        fprintf(stderr, "[VideoDecoder] avcodec_open2 FAILED (ch=%d): %s (%d)\n",
                channel_id_, errbuf, open_ret);
        if (hw_pre_reserved) s_hw_decoder_count.fetch_sub(1, std::memory_order_relaxed);
        avcodec_free_context(&cdc_ctx_);
        cdc_ctx_ = nullptr;
        return false;
    }
    fprintf(stderr, "[VideoDecoder] avcodec_open2 OK (ch=%d)\n", channel_id_);
    fflush(stderr);
    using_hw_decoder_ = true;
    if (!decoder_mode_counted_) {
        decoder_mode_counted_ = true;
    }

    {
        size_t w = static_cast<size_t>(cdc_ctx_->width);
        size_t h = static_cast<size_t>(cdc_ctx_->height);
        size_t bytes_per_frame = w * h * 3 / 2;
        size_t surfaces = static_cast<size_t>(std::max(6, cdc_ctx_->extra_hw_frames + 4));
        decoder_vram_bytes_ = bytes_per_frame * surfaces;
        s_total_decoder_vram_bytes.fetch_add(decoder_vram_bytes_, std::memory_order_relaxed);
    }

    return true;
}

// 持续错误时停止本通道解码
void VideoDecoder::fallbackToSoftware(const char* reason)
{
    fprintf(stderr, "[VideoDecoder] ch=%d: HW persistent error (%s), stopping channel.\n",
            channel_id_, reason ? reason : "unknown");
    is_running_ = false;
}

void VideoDecoder::updateDecoderStatsOnClose()
{
    if (decoder_vram_bytes_ > 0) {
        s_total_decoder_vram_bytes.fetch_sub(decoder_vram_bytes_, std::memory_order_relaxed);
        decoder_vram_bytes_ = 0;
    }
    if (decoder_mode_counted_) {
        if (using_hw_decoder_) s_hw_decoder_count.fetch_sub(1, std::memory_order_relaxed);
        else s_sw_decoder_count.fetch_sub(1, std::memory_order_relaxed);
        decoder_mode_counted_ = false;
    }
    using_hw_decoder_ = false;
}

// 格式协商回调
enum AVPixelFormat VideoDecoder::get_hw_format(AVCodecContext *ctx, const enum AVPixelFormat *pix_fmts)
{
    const enum AVPixelFormat *p;
    for (p = pix_fmts; *p != -1; p++)
    {
        if (*p == AV_PIX_FMT_CUDA)
        {
            return *p; // 找到 CUDA 格式，选中它
        }
    }
    fprintf(stderr, "Failed to get HW surface format. Fallback to software.\n");
    return ctx->sw_pix_fmt; // 回退到软件格式
}

void VideoDecoder::startDecoding()
{
    if (!det_upload_stream_) {
        if (cudaStreamCreateWithFlags(&det_upload_stream_, cudaStreamNonBlocking) != cudaSuccess) {
            det_upload_stream_ = nullptr;
        }
    }

    if (is_image_mode_) {
        processImageSource();
        return;
    }

    // 1. 初始化硬件 (CUDA)
    if (!initHardware())
        return;

    // 2. 打开 Demuxer
    if (!demuxer_.open(url_))
    {
        std::cerr << "Failed to open demuxer: " << url_ << std::endl;
        return;
    }

    // 判断是否为文件模式 (用于 seek 和 speed 控制)
    // 排除 rtsp/rtmp 流和 USB 摄像头 (/dev/video)
    is_file_mode_ = (url_.find("rtsp://") == std::string::npos &&
                     url_.find("rtmp://") == std::string::npos &&
                     url_.find("/dev/video") == std::string::npos);

    // 3. 获取流参数并创建解码器
    AVCodecParameters *codecpar = demuxer_.getCodecParams();
    if (!codecpar)
        return;

    codecpar_ = codecpar;
    codec_ = avcodec_find_decoder(codecpar_->codec_id);
    if (!codec_)
    {
        std::cerr << "Codec not found." << std::endl;
        return;
    }

    hw_decode_enabled_ = true;
    fprintf(stderr, "[VideoDecoder] startDecoding(ch=%d): opening HW codec\n", channel_id_);
    if (!openCodec()) {
        fprintf(stderr, "[VideoDecoder] startDecoding(ch=%d): HW codec failed (NVDEC limit?), giving up\n", channel_id_);
        return;
    }
    fprintf(stderr, "[VideoDecoder] startDecoding(ch=%d): codec opened OK (hw_enabled=%d, using_hw=%d)\n",
            channel_id_, hw_decode_enabled_ ? 1 : 0, using_hw_decoder_ ? 1 : 0);

    // 4. 启动 Demuxer 线程 (生产者开始工作)
    fprintf(stderr, "[VideoDecoder] ch=%d: starting demuxer...\n", channel_id_);
    fflush(stderr);
    demuxer_.start();
    is_running_ = true;
    fprintf(stderr, "[VideoDecoder] ch=%d: demuxer started, entering decode loop\n", channel_id_);
    fflush(stderr);

    // 5. 准备 Frame 容器
    AVFrame *frame = av_frame_alloc(); // 存放解码后的数据 (可能是 GPU 内存)
    int frame_counter = 0;             // 用于跳帧计数

    // 纯 HW 模式：无 CPU↔GPU 上传 lambda（SW 解码已移除）

    int hw_error_streak = 0;
    int kHwFallbackThreshold = 10;
    if (const char* env = std::getenv("CUDAFORGE_HW_FALLBACK_STREAK")) {
        int v = std::atoi(env);
        if (v > 0) kHwFallbackThreshold = std::clamp(v, 3, 100);
    }

    fprintf(stderr, "[VideoDecoder] ch=%d: about to enter consumer loop (hw=%d)\n", channel_id_, hw_decode_enabled_ ? 1 : 0);
    fflush(stderr);
    // --- 消费者循环 ---
    while (is_running_)
    {
        // 处理暂停
        if (is_paused_)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        // 1. 从队列取包
        auto t_pop_start = std::chrono::steady_clock::now();
        AVPacket *pkt = queue_.pop();
        auto t_pop_end = std::chrono::steady_clock::now();
        PipelineStats::getInstance().decode_pop_wait_us.fetch_add(
            static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(t_pop_end - t_pop_start).count()),
            std::memory_order_relaxed);
        if (!pkt)
        {
            // 队列返回空，说明停止了或者文件读完了
            if (!is_running_) {
                // 真正停止了，退出循环
                break;
            }
            
            if (is_file_mode_) {
                // 文件播放结束，发出信号
                Q_EMIT playbackFinished(channel_id_);
                
                // 不退出循环，而是等待 seek 唤醒
                is_eof_.store(true);
                std::unique_lock<std::mutex> lock(eof_mutex_);
                eof_cv_.wait(lock, [this]() { return !is_eof_.load() || !is_running_; });
                
                if (!is_running_) break; // 被停止了，退出
                continue; // 被 seek 唤醒，继续循环
            }
            break;
        }

        // 更新当前播放位置
        if (pkt->pts != AV_NOPTS_VALUE) {
            AVRational tb = demuxer_.getTimeBase();
            int64_t pts_ms = av_rescale_q(pkt->pts, tb, {1, 1000});
            current_position_ms_.store(pts_ms, std::memory_order_relaxed);
        }

        // 2. 发送给解码器
        int ret = 0;
        {
            auto decodeRange = nvtxutil::ScopedRange(
                nvtxutil::makeStageLabel("Decode", channel_id_),
                nvtxutil::color::Decode);
            PipelineStats::getInstance().packets_popped.fetch_add(1, std::memory_order_relaxed);
            auto t_send_start = std::chrono::steady_clock::now();
            {
                std::unique_lock<std::mutex> lk(codec_mutex_);
                // 再次检查 ctx 并在发送前验证 packet
                if (!cdc_ctx_ || !avcodec_is_open(cdc_ctx_)) {
                    lk.unlock();
                    av_packet_free(&pkt);
                    break;
                }
                // 只拦截"有 size 但 data 为空"的真正损坏包
                // 注意：pkt->size==0 && !pkt->data 是 FFmpeg flush packet（EOF刷新），必须放行
                // 注意：不能在 av_packet_free 之后再访问 pkt->size，否则 null deref → SIGSEGV
                if (pkt->size > 0 && !pkt->data) {
                    int bad_size = pkt->size;
                    lk.unlock();
                    av_packet_free(&pkt);
                    fprintf(stderr, "[VideoDecoder] Error: Invalid packet data (size=%d, data=null), dropped.\n", bad_size);
                    continue;
                }
                ret = avcodec_send_packet(cdc_ctx_, pkt);
            }
            auto t_send_end = std::chrono::steady_clock::now();
            PipelineStats::getInstance().decode_send_us.fetch_add(
                static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(t_send_end - t_send_start).count()),
                std::memory_order_relaxed);
            av_packet_free(&pkt); // 归还包内存

            if (ret < 0)
            {char errbuf[AV_ERROR_MAX_STRING_SIZE] = {0};
                av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
                std::cerr << "[VideoDecoder] Error sending packet: " << errbuf << " (" << ret << ")" << std::endl;
                
                if (ret == AVERROR(EAGAIN)) {
                    // 解码器暂时背压，继续走 receive 流程释放 surface
                    ret = 0;
                } else if (ret == AVERROR_INVALIDDATA) {
                    // "No decoder surfaces left" 也以 AVERROR_INVALIDDATA 返回
                    // 此时必须进入 receive 循环释放已解码帧（归还 surface），否则 surface 永远无法回收
                    // 旧代码用 continue 跳过了 receive → 形成死循环 → surface 耗尽 → 崩溃
                    if (hw_decode_enabled_) {
                        hw_error_streak++;
                        std::cerr << "[VideoDecoder] send_packet transient error, streak="
                                  << hw_error_streak << "/" << kHwFallbackThreshold << std::endl;
                        if (hw_error_streak >= kHwFallbackThreshold) {
                            std::cerr << "Error sending packet for decoding (persistent)." << std::endl;
                            fallbackToSoftware("send_packet persistent invaliddata");
                            hw_error_streak = 0;
                        }
                    }
                    ret = 0;  // 落入 receive 循环以释放 surface
                } else {
                    if (hw_decode_enabled_) {
                        hw_error_streak++;
                        if (hw_error_streak >= kHwFallbackThreshold) {
                            std::cerr << "Error sending packet for decoding (persistent)." << std::endl;
                            fallbackToSoftware("send_packet persistent error");
                            hw_error_streak = 0;
                        }
                    }
                    ret = 0;  // 仍然尝试 receive 已解码帧
                }
            } else {
                hw_error_streak = 0;
            }

            // 3. 接收解码后的帧 (可能一次 send 对应多次 receive)
            auto t_recv_start = std::chrono::steady_clock::now();
            while (ret >= 0)
            {
                {
                    std::unique_lock<std::mutex> lk(codec_mutex_);
                    if (!cdc_ctx_) {
                        lk.unlock();
                        ret = AVERROR(EAGAIN);
                    } else {
                        ret = avcodec_receive_frame(cdc_ctx_, frame);
                    }
                }
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                    break;
                if (ret < 0) {
                    if (hw_decode_enabled_) {
                        hw_error_streak++;
                        if (hw_error_streak >= kHwFallbackThreshold) {
                            fallbackToSoftware("receive_frame persistent error");
                            hw_error_streak = 0;
                        }
                    }
                    break;
                }

                hw_error_streak = 0;

                // --- 智能自适应跳帧逻辑 ---
                double native_fps = getNativeFPS();
                int skip_step = 1;

                // 计算实际目标 FPS：网格模式固定 30，详情模式使用 target_fps_，并乘以播放速率（例如 2x -> 120）
                double target = (low_fps_mode_ ? 30.0 : static_cast<double>(target_fps_.load()));
                target *= playback_speed_; // playback_speed_ 默认为 1.0，在文件模式下可调整
                if (target < 1.0) target = 1.0;

                if (native_fps > target)
                {
                    skip_step = std::max(1, (int)std::round(native_fps / target));
                }

                if (frame_counter++ % skip_step != 0)
                {
                    av_frame_unref(frame);
                    continue;
                }

                // 兼容 SW 解码输出：非 CUDA 帧转成 CUDA NV12，避免直接丢帧
                if (frame->format != AV_PIX_FMT_CUDA) {
                    int fw = frame->width;
                    int fh = frame->height;
                    int64_t fpts = frame->pts;
                    size_t nv12_size = static_cast<size_t>(fw) * fh * 3 / 2;
                    uint8_t* d_buf = nullptr;
                    cudaError_t alloc_err = cudaMalloc(&d_buf, nv12_size);
                    if (alloc_err != cudaSuccess) {
                        fprintf(stderr, "[VideoDecoder] ch=%d: cudaMalloc for SW frame (%zu B) failed: %s\n",
                                channel_id_, nv12_size, cudaGetErrorString(alloc_err));
                        av_frame_unref(frame);
                        continue;
                    }

                    bool converted = false;
                    if (frame->format == AV_PIX_FMT_NV12 && frame->data[0] && frame->data[1]) {
                        cudaError_t e1 = cudaMemcpy2D(d_buf, fw,
                                                      frame->data[0], frame->linesize[0],
                                                      fw, fh, cudaMemcpyHostToDevice);
                        cudaError_t e2 = cudaMemcpy2D(d_buf + static_cast<size_t>(fw) * fh, fw,
                                                      frame->data[1], frame->linesize[1],
                                                      fw, fh / 2, cudaMemcpyHostToDevice);
                        converted = (e1 == cudaSuccess && e2 == cudaSuccess);
                    } else if (frame->format == AV_PIX_FMT_YUV420P && frame->data[0] && frame->data[1] && frame->data[2]) {
                        std::vector<uint8_t> nv12_host(nv12_size);
                        uint8_t* y_plane = nv12_host.data();
                        uint8_t* uv_plane = y_plane + static_cast<size_t>(fw) * fh;

                        for (int y = 0; y < fh; ++y) {
                            std::memcpy(y_plane + static_cast<size_t>(y) * fw,
                                        frame->data[0] + static_cast<size_t>(y) * frame->linesize[0],
                                        static_cast<size_t>(fw));
                        }
                        for (int y = 0; y < fh / 2; ++y) {
                            const uint8_t* src_u = frame->data[1] + static_cast<size_t>(y) * frame->linesize[1];
                            const uint8_t* src_v = frame->data[2] + static_cast<size_t>(y) * frame->linesize[2];
                            uint8_t* dst_uv = uv_plane + static_cast<size_t>(y) * fw;
                            for (int x = 0; x < fw / 2; ++x) {
                                dst_uv[2 * x] = src_u[x];
                                dst_uv[2 * x + 1] = src_v[x];
                            }
                        }

                        cudaError_t e = cudaMemcpy(d_buf, nv12_host.data(), nv12_size, cudaMemcpyHostToDevice);
                        converted = (e == cudaSuccess);
                    }

                    if (!converted) {
                        fprintf(stderr, "[VideoDecoder] ch=%d: non-CUDA frame convert failed (fmt=%d), dropping.\n",
                                channel_id_, frame->format);
                        cudaFree(d_buf);
                        av_frame_unref(frame);
                        continue;
                    }

                    av_frame_unref(frame);
                    frame->format = AV_PIX_FMT_CUDA;
                    frame->width = fw;
                    frame->height = fh;
                    frame->pts = fpts;

                    CudaBufMeta* meta = new CudaBufMeta();
                    meta->bytes = nv12_size;
                    frame->buf[0] = av_buffer_create(d_buf, static_cast<int>(nv12_size),
                                                     cuda_buf_free, meta, 0);
                    if (!frame->buf[0]) {
                        delete meta;
                        cudaFree(d_buf);
                        av_frame_unref(frame);
                        continue;
                    }
                    registerStandaloneFrameAlloc(nv12_size);
                    frame->data[0] = d_buf;
                    frame->data[1] = d_buf + static_cast<size_t>(fw) * fh;
                    frame->linesize[0] = fw;
                    frame->linesize[1] = fw;
                }

                // === [关键修复] 立即将 NVDEC surface 拷贝到独立 CUDA 缓冲 ===
                // 问题：av_frame_clone 引用 NVDEC surface，FrameQueue/输入队列持有时间过长
                //       导致 "No decoder surfaces left" → decode 失败级联崩溃
                // 方案：D2D memcpy 后立即 av_frame_unref，NVDEC surface 瞬间归还
                {
                    auto frameCopyRange = nvtxutil::ScopedRange(
                        nvtxutil::makeStageLabel("DecodeFrameCopy", channel_id_),
                        nvtxutil::color::Decode);
                    int fw = frame->width, fh = frame->height;
                    int64_t fpts = frame->pts;
                    size_t nv12_size = (size_t)fw * fh * 3 / 2;
                    uint8_t* d_buf = nullptr;
                    cudaError_t alloc_err = cudaMalloc(&d_buf, nv12_size);
                    if (alloc_err != cudaSuccess) {
                        fprintf(stderr, "[VideoDecoder] ch=%d: cudaMalloc standalone NV12 (%zu B) failed: %s\n",
                                channel_id_, nv12_size, cudaGetErrorString(alloc_err));
                        av_frame_unref(frame);
                        continue;
                    }
                    // Y 平面
                    cudaMemcpy2D(d_buf, fw,
                                 frame->data[0], frame->linesize[0],
                                 fw, fh, cudaMemcpyDeviceToDevice);
                    // UV 平面
                    cudaMemcpy2D(d_buf + (size_t)fw * fh, fw,
                                 frame->data[1], frame->linesize[1],
                                 fw, fh / 2, cudaMemcpyDeviceToDevice);

                    // 释放 NVDEC surface（引用计数 -1，surface 立即归还给硬件解码器）
                    av_frame_unref(frame);

                    // 用独立 CUDA 缓冲重新填充 frame，AVBufferRef 负责生命周期
                    frame->format = AV_PIX_FMT_CUDA;
                    frame->width  = fw;
                    frame->height = fh;
                    frame->pts    = fpts;
                    CudaBufMeta* meta = new CudaBufMeta();
                    meta->bytes = nv12_size;
                    frame->buf[0] = av_buffer_create(d_buf, (int)nv12_size,
                                                      cuda_buf_free, meta, 0);
                    if (!frame->buf[0]) {
                        delete meta;
                        cudaFree(d_buf);
                        av_frame_unref(frame);
                        continue;
                    }
                    registerStandaloneFrameAlloc(nv12_size);
                    frame->data[0]     = d_buf;
                    frame->data[1]     = d_buf + (size_t)fw * fh;
                    frame->linesize[0] = fw;
                    frame->linesize[1] = fw;
                }

                PipelineStats::getInstance().frames_decoded.fetch_add(1, std::memory_order_relaxed);
                AVFrame *frame_clone = av_frame_clone(frame);
                if (frame_clone) {
                    auto dispatchRange = nvtxutil::ScopedRange(
                        nvtxutil::makeStageLabel("DispatchFrame", channel_id_),
                        nvtxutil::color::Control);
                    // push 是阻塞的。如果队列满了，这里会卡住，直到 DisplayManager 取走一帧。
                    // 这就实现了“显示端控制速率”。
                    auto t_fq_push_start = std::chrono::steady_clock::now();
                    // 实时源不应反压解码线程，否则会导致 NVDEC surface 不足。
                    // 文件源保持阻塞 push 以保证逐帧顺序与完整性。
                    bool fq_ok = is_file_mode_
                        ? frame_queue_->push(frame_clone)
                        : frame_queue_->pushDropOldest(frame_clone);
                    auto t_fq_push_end = std::chrono::steady_clock::now();
                    PipelineStats::getInstance().framequeue_push_wait_us.fetch_add(
                        static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(t_fq_push_end - t_fq_push_start).count()),
                        std::memory_order_relaxed);
                    if (!fq_ok) {
                        av_frame_free(&frame_clone); // 如果 push 返回 false (停止了)，释放内存
                        PipelineStats::getInstance().frames_dropped_fq.fetch_add(1, std::memory_order_relaxed);
                        break; // 队列已停，立即跳出内层接收循环，防止刷屏日志
                    }
                    PipelineStats::getInstance().frames_pushed_fq.fetch_add(1, std::memory_order_relaxed);

                    // 仅在 FrameQueue push 成功后才尝试推入输入 Arena
                    // 直接在源侧完成颜色转换 + resize + letterbox，避免先做一次 NV12 letterbox，
                    // Worker 再重复做一次预处理。
                    if (InputFrameArenaStore::getInstance().isChannelEnabled(channel_id_)) {
                        enqueueDetectionTensorFromNV12Frame(frame);
                    } else if (is_image_mode_) {
                        fprintf(stderr, "[VideoDecoder] WARNING: Image ch=%d detection channel disabled, frame NOT pushed to input arena\n",
                                channel_id_);
                    }
                } else {
                    // 如果 frame_clone 失败（例如 EOF 或内存不足），立即退出循环
                    break;
                }

                av_frame_unref(frame); // 释放引用，准备接收下一帧
            }
            auto t_recv_end = std::chrono::steady_clock::now();
            PipelineStats::getInstance().decode_receive_us.fetch_add(
                static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(t_recv_end - t_recv_start).count()),
                std::memory_order_relaxed);
        }
    }

    av_frame_free(&frame);

    // 退出解码线程时再释放 codec，避免并发使用导致崩溃
    {
        std::lock_guard<std::mutex> lk(codec_mutex_);
        updateDecoderStatsOnClose();
        if (cdc_ctx_) {
            avcodec_flush_buffers(cdc_ctx_);
            avcodec_free_context(&cdc_ctx_);
            cdc_ctx_ = nullptr;
        }
    }
}
