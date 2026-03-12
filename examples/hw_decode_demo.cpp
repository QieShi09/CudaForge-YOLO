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

static enum AVPixelFormat get_hw_format(AVCodecContext *ctx,
                                        const enum AVPixelFormat *pix_fmts) {
    // 遍历所有支持的格式
    for (const enum AVPixelFormat *p = pix_fmts; *p != -1; p++) {
        // 检查是否是硬件格式
        if (*p == AV_PIX_FMT_CUDA ||     // NVIDIA
            *p == AV_PIX_FMT_VAAPI ||    // Intel/AMD
            *p == AV_PIX_FMT_DXVA2_VLD || // Windows
            *p == AV_PIX_FMT_VIDEOTOOLBOX) { // macOS
            return *p;  // ✅ 找到硬件格式，返回
        }
    }
    
    // 没有硬件格式，返回第一个软件格式作为fallback
    return pix_fmts[0];
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

    // 如果希望尝试使用 HW device（例如 CUDA），可在此处创建并关联：
    // AVBufferRef* hw_device_ctx = nullptr;
    // av_hwdevice_ctx_create(&hw_device_ctx, AV_HWDEVICE_TYPE_CUDA, nullptr, nullptr, 0);
    // dec_ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);
    // dec_ctx->get_format = get_hw_format;
    // 注意：并非所有 FFmpeg 构建都支持 AV_HWDEVICE_TYPE_CUDA，这里仅作演示。

    if (avcodec_open2(dec_ctx, decoder, nullptr) < 0) { std::cerr << "Failed to open codec\n"; avcodec_free_context(&dec_ctx); avformat_close_input(&fmt); return false; }

    *outFmtCtx = fmt; *outDecCtx = dec_ctx; *outVideoStream = video_stream;
    return true;
}

// 读取并解码若干帧（打印信息）。如果解码到软件帧，可使用 sws_scale 转换到 RGB 以便显示/保存。
void decode_loop(AVFormatContext* fmtCtx, AVCodecContext* decCtx, int video_stream_index, int max_frames) {
    AVPacket* pkt = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    AVFrame* sw_frame = av_frame_alloc();
    int got = 0; int count = 0;

    while (av_read_frame(fmtCtx, pkt) >= 0 && count < max_frames) {
        if (pkt->stream_index == video_stream_index) {
            if (avcodec_send_packet(decCtx, pkt) == 0) {
                while (avcodec_receive_frame(decCtx, frame) == 0) {
                    // 如果是硬件帧，需要将其转为可用的 software frame（示例中尽量使用 sw_frame）
                    if (frame->format == AV_PIX_FMT_NONE) {
                        // 仅示例：检测并说明
                        std::cout << "Received a hardware frame (unsupported format id).\n";
                    }

                    // 打印 PTS / 时基信息
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

    // flush
    avcodec_send_packet(decCtx, nullptr);
    while (avcodec_receive_frame(decCtx, frame) == 0) {
        std::cout << "Flushed frame format=" << frame->format << "\n";
    }

    av_frame_free(&frame);
    av_frame_free(&sw_frame);
    av_packet_free(&pkt);
}

int main(int argc, char** argv) {
    if (argc < 2) { std::cout << "Usage: hw_decode_demo <video>
"; return 0; }
    AVFormatContext* fmt = nullptr; AVCodecContext* dec = nullptr; int vid = -1;
    if (!open_input(argv[1], &fmt, &dec, &vid)) return 1;
    std::cout << "Opened " << argv[1] << ", decoding 10 frames...\n";
    decode_loop(fmt, dec, vid, 10);
    avcodec_free_context(&dec);
    avformat_close_input(&fmt);
    return 0;
}

