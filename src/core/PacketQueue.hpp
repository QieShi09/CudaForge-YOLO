#ifndef PACKET_QUEUE_HPP
#define PACKET_QUEUE_HPP

#include <queue>
#include <mutex>
#include <condition_variable>

extern "C" {
#include <libavcodec/avcodec.h>
}

class PacketQueue {
public:
    void push(AVPacket* pkt);
    AVPacket* pop();
    void clear();
    void stop();
    void start();
    bool isEmpty();

private:
    std::queue<AVPacket*> queue_;
    std::mutex mutex_;
    std::condition_variable cond_;
    bool stop_flag_ = false;
};

#endif // PACKET_QUEUE_HPP