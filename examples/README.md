
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

