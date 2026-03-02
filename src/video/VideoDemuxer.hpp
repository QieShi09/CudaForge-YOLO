#ifndef VIDEO_DEMUXER_HPP
#define VIDEO_DEMUXER_HPP

#include "src/core/PacketQueue.hpp"
#include <thread>
#include <atomic>
#include <string>
extern "C" {
#include <libavformat/avformat.h>
}

class VideoDemuxer {
public:
    VideoDemuxer(PacketQueue* queue);
    ~VideoDemuxer();

    bool open(const std::string& url);
    void start();
    void stop();
    void seek(int64_t timestamp_ms);
    AVCodecParameters* getCodecParams();
    int64_t getDuration() const; // 获取时长
    AVRational getFrameRate();   // 获取原始帧率
    AVRational getTimeBase();    // 获取时间基
    bool isOpen() const;         // 是否已打开输入
    void setSpeed(float speed);  // 设置播放速率
    bool isEOF() const { return is_eof_.load(); } // 是否已结束
    void resetEOF();             // 重置 EOF 状态（用于 seek/replay）
    void setPaused(bool paused); // 暂停/恢复读取

private:
    void demuxLoop();
    
    // 中断回调函数，用于打断阻塞的 av_read_frame
    static int interrupt_cb(void* ctx);
    PacketQueue* queue_; // 不负责管理内存，只负责用
    AVFormatContext* fmt_ctx_ = nullptr;
    std::string url_;
    int video_stream_index_ = -1;
    
    std::thread worker_thread_;
    std::atomic<bool> is_running_{false};
    std::atomic<bool> abort_request_{false}; // 强制中止标志
    std::atomic<float> speed_{1.0f};         // 播放速率
    std::atomic<bool> is_eof_{false};        // 是否已结束
    std::atomic<bool> is_paused_{false};     // 暂停标志
};

#endif // VIDEO_DEMUXER_HPP