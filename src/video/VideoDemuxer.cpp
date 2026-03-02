#include "VideoDemuxer.hpp"
#include "../core/PipelineStats.hpp"
#include <iostream>
#include <chrono>

extern "C" {
#ifdef HAVE_AVDEVICE
#include <libavdevice/avdevice.h>
#endif
}

VideoDemuxer::VideoDemuxer(PacketQueue* queue) : queue_(queue) {}

// 中断回调实现
int VideoDemuxer::interrupt_cb(void* ctx) {
    VideoDemuxer* demuxer = static_cast<VideoDemuxer*>(ctx);
    if (demuxer->abort_request_) return 1; // 返回 1 表示中断操作
    return 0;
}

VideoDemuxer::~VideoDemuxer() {
    stop();
    if (fmt_ctx_) {
        avformat_close_input(&fmt_ctx_);
    }
}

bool VideoDemuxer::open(const std::string& url) {
    url_ = url;
    abort_request_ = false; // 重置中断标志

#ifdef HAVE_AVDEVICE
#else
    std::cerr << "[VideoDemuxer] CRITICAL WARNING: Build missing libavdevice. Camera will NOT work." << std::endl;
#endif
    
    // 注册所有设备 (确保能找到 video4linux2)
#ifdef HAVE_AVDEVICE
    static bool device_registered = false;
    if (!device_registered) {
        avdevice_register_all();
        device_registered = true;
    }
#endif

    // 如果是 Linux 设备路径，强制指定 v4l2 格式
    // FFmpeg 无法自动探测 /dev/video0 这种设备文件，必须显式指定格式
    AVInputFormat* ifmt = nullptr;
#ifdef HAVE_AVDEVICE
    if (url.find("/dev/video") != std::string::npos) {
        ifmt = av_find_input_format("video4linux2");
        if (!ifmt) {
            std::cerr << "[VideoDemuxer] Warning: video4linux2 input format not found." << std::endl;
        }
    }
#else
    if (url.find("/dev/video") != std::string::npos) {
        std::cerr << "[VideoDemuxer] Warning: libavdevice not found during build. Camera support disabled." << std::endl;
    }
#endif

    // 分配上下文并设置中断回调
    fmt_ctx_ = avformat_alloc_context();
    fmt_ctx_->interrupt_callback.callback = interrupt_cb;
    fmt_ctx_->interrupt_callback.opaque = this;

    // 设置输入选项（针对不同源类型）
    AVDictionary* options = nullptr;
    if (url.find("/dev/video") != std::string::npos) {
        // USB 摄像头：请求 30fps 和 1280x720 分辨率
        av_dict_set(&options, "framerate", "30", 0);
        av_dict_set(&options, "video_size", "1280x720", 0);
        // 使用 MJPEG 格式（大多数摄像头支持更高帧率）
        av_dict_set(&options, "input_format", "mjpeg", 0);
    } else if (url.find("rtsp://") != std::string::npos) {
        // RTSP：使用 TCP 传输，更稳定
        av_dict_set(&options, "rtsp_transport", "tcp", 0);
        av_dict_set(&options, "stimeout", "5000000", 0); // 5秒超时
    }

    // 打开输入：先尝试使用 ifmt + options（针对 /dev/video 强制 v4l2 + input_format）
    if (avformat_open_input(&fmt_ctx_, url.c_str(), ifmt, &options) < 0) {
        std::cerr << "[VideoDemuxer] Failed to open input with forced format/options: " << url << std::endl;
        av_dict_free(&options);

        // 回退策略：如果是设备文件，重试不强制 input_format（部分摄像头不使用 mjpeg）
        if (url.find("/dev/video") != std::string::npos) {
            AVDictionary* fallback_opts = nullptr;
            av_dict_set(&fallback_opts, "framerate", "30", 0);
            av_dict_set(&fallback_opts, "video_size", "1280x720", 0);

            std::cerr << "[VideoDemuxer] Retrying open without input_format for device: " << url << std::endl;
            if (avformat_open_input(&fmt_ctx_, url.c_str(), nullptr, &fallback_opts) < 0) {
                std::cerr << "[VideoDemuxer] Fallback open failed: " << url << std::endl;
                av_dict_free(&fallback_opts);
                return false;
            }
            av_dict_free(&fallback_opts);
        } else {
            return false;
        }
    } else {
        av_dict_free(&options);
    }

    if (avformat_find_stream_info(fmt_ctx_, nullptr) < 0) {
        std::cerr << "[VideoDemuxer] Failed to find stream info." << std::endl;
        return false;
    }

    // 查找视频流
    video_stream_index_ = -1;
    for (unsigned int i = 0; i < fmt_ctx_->nb_streams; i++) {
        if (fmt_ctx_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index_ = i;
            break;
        }
    }

    if (video_stream_index_ == -1) {
        std::cerr << "[VideoDemuxer] No video stream found." << std::endl;
        return false;
    }

    return true;
}

void VideoDemuxer::start() {
    if (is_running_) return;
    is_running_ = true;
    worker_thread_ = std::thread(&VideoDemuxer::demuxLoop, this);
}

void VideoDemuxer::stop() {
    is_running_ = false;
    abort_request_ = true; // 通知 FFmpeg 中断阻塞操作
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
}

void VideoDemuxer::seek(int64_t timestamp_ms) {
    if (!fmt_ctx_) return;
    
    // 如果 demuxer 已经停止（EOF 或手动停止），需要重新启动
    bool wasRunning = is_running_.load();
    bool wasEOF = is_eof_.load();
    
    // 先停止旧线程（如果还在运行）
    if (wasEOF) {
        // EOF 后线程已经结束，需要等待并清理
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
    }
    
    // 执行 seek
    int64_t ts = timestamp_ms * 1000; 
    av_seek_frame(fmt_ctx_, -1, ts, AVSEEK_FLAG_BACKWARD);
    
    // 清空队列并重新启用
    if (queue_) {
        queue_->clear();
        queue_->start(); // 重新启用队列
    }
    
    // 重置 EOF 标志
    is_eof_.store(false);
    
    // 如果之前是 EOF 状态，需要重新启动 demuxer 线程
    if (wasEOF) {
        is_running_ = true;
        abort_request_ = false;
        worker_thread_ = std::thread(&VideoDemuxer::demuxLoop, this);
    }
}

void VideoDemuxer::resetEOF() {
    is_eof_.store(false);
    if (queue_) queue_->start();
}

AVCodecParameters* VideoDemuxer::getCodecParams() {
    if (video_stream_index_ >= 0 && fmt_ctx_) {
        return fmt_ctx_->streams[video_stream_index_]->codecpar;
    }
    return nullptr;
}

int64_t VideoDemuxer::getDuration() const {
    if (fmt_ctx_ && fmt_ctx_->duration != AV_NOPTS_VALUE) {
        return fmt_ctx_->duration / (AV_TIME_BASE / 1000); // ms
    }
    return 0;
}

AVRational VideoDemuxer::getFrameRate() {
    if (video_stream_index_ >= 0 && fmt_ctx_) {
        return fmt_ctx_->streams[video_stream_index_]->avg_frame_rate;
    }
    return {30, 1}; // 默认 30
}

AVRational VideoDemuxer::getTimeBase() {
    if (video_stream_index_ >= 0 && fmt_ctx_) {
        return fmt_ctx_->streams[video_stream_index_]->time_base;
    }
    return AVRational{1, 1000}; // 默认
}

bool VideoDemuxer::isOpen() const {
    return fmt_ctx_ != nullptr;
}

void VideoDemuxer::setSpeed(float speed) {
    if (speed > 0.1f && speed <= 10.0f) {
        speed_.store(speed, std::memory_order_relaxed);
    }
}

void VideoDemuxer::setPaused(bool paused) {
    is_paused_.store(paused, std::memory_order_relaxed);
}

void VideoDemuxer::demuxLoop() {
    AVPacket* pkt = av_packet_alloc();
    
    // 获取视频原生帧率，计算基准帧间隔
    AVRational frame_rate = getFrameRate();
    int base_interval_ms = (frame_rate.num > 0) ? (1000 * frame_rate.den / frame_rate.num) : 33;

    is_eof_.store(false); // 重置 EOF 标志

    while (is_running_) {
        // 处理暂停状态
        if (is_paused_.load(std::memory_order_relaxed)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        auto start_time = std::chrono::steady_clock::now();

        auto t_read_start = std::chrono::steady_clock::now();
        int ret = av_read_frame(fmt_ctx_, pkt);
        auto t_read_end = std::chrono::steady_clock::now();
        PipelineStats::getInstance().demux_read_us.fetch_add(
            static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(t_read_end - t_read_start).count()),
            std::memory_order_relaxed);
        if (ret < 0) {
            if (ret == AVERROR_EOF) {
                // 文件播放结束
                is_eof_.store(true);
                queue_->stop(); // 通知消费者队列结束
                break;
            }
            // 其他错误，休眠后重试
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        if (pkt->stream_index == video_stream_index_) {
            PipelineStats::getInstance().demux_packets_read.fetch_add(1, std::memory_order_relaxed);
            queue_->push(av_packet_clone(pkt));
        }
        av_packet_unref(pkt);

        // 根据播放速率调整帧间隔
        float current_speed = speed_.load(std::memory_order_relaxed);
        int target_interval_ms = static_cast<int>(base_interval_ms / current_speed);
        if (target_interval_ms < 1) target_interval_ms = 1;

        auto end_time = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
        if (elapsed < target_interval_ms) {
            std::this_thread::sleep_for(std::chrono::milliseconds(target_interval_ms - elapsed));
        }
    }
    av_packet_free(&pkt);
}