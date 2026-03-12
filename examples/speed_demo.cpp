#include <iostream>
#include <thread>
#include <chrono>

// 简易播放控制示例：基于帧间隔控制“播放速度”。
// 这个示例并不连接解码，只展示如何按帧时间推进与暂停/倍速逻辑。

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
    double frame_rate = 25.0; // 假设 25 FPS
    double frame_interval_ms = 1000.0 / frame_rate;
    int frame = 0;
    ctrl.set_speed(2.0); // 2x

    for (;;) {
        if (!ctrl.paused()) {
            // 模拟处理一帧
            std::cout << "Displaying frame " << frame << " at speed=" << ctrl.speed() << "x\n";
            ++frame;
        }
        // 等待下帧：按 speed 缩放帧间间隔
        double wait_ms = frame_interval_ms / ctrl.speed();
        std::this_thread::sleep_for(std::chrono::milliseconds((int)wait_ms));
        if (frame >= 100) break;
    }
    return 0;
}
