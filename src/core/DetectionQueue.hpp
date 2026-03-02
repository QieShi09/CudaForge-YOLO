#ifndef DETECTION_QUEUE_HPP
#define DETECTION_QUEUE_HPP

#include <deque>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <cstddef>
#include <chrono>
#include <unordered_set>

extern "C" {
#include <libavutil/frame.h>
}

/** 线程安全的检测帧队列（单例），提供有界 push/pop 操作。
 */
class DetectionQueue {
public:
	static DetectionQueue& getInstance() {
		static DetectionQueue inst;
		return inst;
	}

	void init(size_t max_items) {
		std::lock_guard<std::mutex> lk(m_);
		max_items_ = max_items;
		stop_ = false;
		disabled_channels_.clear();
		enabled_.store(true);
	}
	void enable() { enabled_.store(true); }
	void disable() { enabled_.store(false); }
	bool isEnabled() const { return enabled_.load(); }

	void disableChannel(int channel_id) {
		std::lock_guard<std::mutex> lk(m_);
		disabled_channels_.insert(channel_id);
	}

	void enableChannel(int channel_id) {
		std::lock_guard<std::mutex> lk(m_);
		disabled_channels_.erase(channel_id);
	}

	bool isChannelEnabled(int channel_id) const {
		if (!isEnabled()) return false;
		std::lock_guard<std::mutex> lk(m_);
		return disabled_channels_.find(channel_id) == disabled_channels_.end();
	}

	// 队列项：包含帧与来源信息
	struct Item {
		AVFrame* frame = nullptr;
		int channel_id = 0;    // 源通道/摄像头 id
		int src_format = 0;    // AVPixelFormat (frame->format)
		int detection_step = 1; // 解码/检测步长 (用于决定是否采样)
		uint64_t epoch = 0;    // 通道 epoch，用于丢弃旧回调
	};

	// 非阻塞 push：当队列达到上限或未启用时返回 false
	bool push(AVFrame* frame, int channel_id = 0, int src_format = 0, int detection_step = 1, uint64_t epoch = 0) {
		if (!frame) return false;
		if (!isEnabled()) return false;
		std::lock_guard<std::mutex> lk(m_);
		if (disabled_channels_.find(channel_id) != disabled_channels_.end()) return false;
		if (queue_.size() >= max_items_) {
			if (overflow_policy_ == OverflowPolicy::DropNew) {
				return false;
			} else {
				// overwrite oldest: 弹出最旧帧并释放，然后插入新帧
				Item old = queue_.front();
				queue_.pop_front();
				if (old.frame) av_frame_free(&old.frame);
			}
		}
		Item it;
		it.frame = frame;
		it.channel_id = channel_id;
		it.src_format = src_format;
		it.detection_step = detection_step;
		it.epoch = epoch;
		queue_.push_back(it);
		cv_.notify_one();
		return true;
	}

    

	// 阻塞 pop：等待直到有帧或 stop 被调用，返回 Item{nullptr,..} 表示已停止且队列空
	Item pop() {
		std::unique_lock<std::mutex> lk(m_);
		cv_.wait(lk, [this]{ return !queue_.empty() || stop_; });
		if (queue_.empty()) return Item();
		Item f = queue_.front();
		queue_.pop_front();
		return f;
	}

	// 带超时的 pop，超时或 stop 返回 Item() (frame == nullptr)
	Item pop_for(std::chrono::milliseconds ms) {
		std::unique_lock<std::mutex> lk(m_);
		if (!cv_.wait_for(lk, ms, [this]{ return !queue_.empty() || stop_; })) return Item();
		if (queue_.empty()) return Item();
		Item f = queue_.front();
		queue_.pop_front();
		return f;
	}

	// 批量弹出：在等待至多 ms 后，最多返回 max_n 个元素（一次加锁，减少争用）
	// 返回空 vector 表示超时或队列停止且为空
	std::vector<Item> pop_bulk(size_t max_n, std::chrono::milliseconds ms) {
		std::unique_lock<std::mutex> lk(m_);
		if (!cv_.wait_for(lk, ms, [this]{ return !queue_.empty() || stop_; })) return {};
		std::vector<Item> out;
		size_t n = std::min(max_n, queue_.size());
		out.reserve(n);
		for (size_t i = 0; i < n; ++i) {
			out.push_back(queue_.front());
			queue_.pop_front();
		}
		return out;
	}

	// 停止队列并唤醒所有等待者
	void stop() {
		std::lock_guard<std::mutex> lk(m_);
		stop_ = true;
		enabled_.store(false);
		cv_.notify_all();
	}

	// 清空队列并释放 AVFrame 引用
	void clear() {
		std::lock_guard<std::mutex> lk(m_);
		while (!queue_.empty()) {
			Item it = queue_.front();
			queue_.pop_front();
			if (it.frame) av_frame_free(&it.frame);
		}
	}

	void clearChannel(int channel_id) {
		std::lock_guard<std::mutex> lk(m_);
		for (auto it = queue_.begin(); it != queue_.end(); ) {
			if (it->channel_id == channel_id) {
				if (it->frame) av_frame_free(&it->frame);
				it = queue_.erase(it);
			} else {
				++it;
			}
		}
	}

	// 当前队列中的帧数（用于监控）
	size_t size() const {
		std::lock_guard<std::mutex> lk(m_);
		return queue_.size();
	}

	// 队列最大容量
	size_t capacity() const { return max_items_; }

private:
	DetectionQueue() = default;
	~DetectionQueue() = default;
	DetectionQueue(const DetectionQueue&) = delete;
	DetectionQueue& operator=(const DetectionQueue&) = delete;

	enum class OverflowPolicy { DropNew = 0, OverwriteOld = 1 };
	OverflowPolicy overflow_policy_ = OverflowPolicy::DropNew;
	std::deque<Item> queue_;
	mutable std::mutex m_;
	std::condition_variable cv_;
	size_t max_items_ = 128;
	std::atomic<bool> enabled_{false};
	std::unordered_set<int> disabled_channels_;
	bool stop_ = false;
};

#endif
