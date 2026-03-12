#include <iostream>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

// 将格式上下文 seek 到指定毫秒时间戳（近似）。
bool seek_to_timestamp(AVFormatContext* fmtCtx, int stream_index, int64_t timestamp_ms) {
    // 将毫秒转换为 AVStream 时间基单位
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
