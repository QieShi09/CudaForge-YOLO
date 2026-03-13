#pragma once

#include <cstdint>
#include <string>
#include <utility>
#ifdef ENABLE_NVTX
#include <nvtx3/nvToolsExt.h>
#endif

namespace nvtxutil {

class ScopedRange {
public:
    ScopedRange(const char* message, std::uint32_t color)
    {
#ifdef ENABLE_NVTX
        nvtxEventAttributes_t attr{};
        attr.version = NVTX_VERSION;
        attr.size = NVTX_EVENT_ATTRIB_STRUCT_SIZE;
        attr.colorType = NVTX_COLOR_ARGB;
        attr.color = color;
        attr.messageType = NVTX_MESSAGE_TYPE_ASCII;
        attr.message.ascii = message;
        id_ = nvtxRangeStartEx(&attr);
#else
        (void)message;
        (void)color;
#endif
    }

    ScopedRange(const std::string& message, std::uint32_t color)
        : owned_message_(message)
    {
#ifdef ENABLE_NVTX
        nvtxEventAttributes_t attr{};
        attr.version = NVTX_VERSION;
        attr.size = NVTX_EVENT_ATTRIB_STRUCT_SIZE;
        attr.colorType = NVTX_COLOR_ARGB;
        attr.color = color;
        attr.messageType = NVTX_MESSAGE_TYPE_ASCII;
        attr.message.ascii = owned_message_.c_str();
        id_ = nvtxRangeStartEx(&attr);
#else
        (void)color;
#endif
    }

    ScopedRange(const ScopedRange&) = delete;
    ScopedRange& operator=(const ScopedRange&) = delete;

    ScopedRange(ScopedRange&& other) noexcept
        : id_(std::exchange(other.id_, 0))
        , owned_message_(std::move(other.owned_message_))
    {}

    ScopedRange& operator=(ScopedRange&& other) noexcept
    {
        if (this != &other) {
            end();
            id_ = std::exchange(other.id_, 0);
            owned_message_ = std::move(other.owned_message_);
        }
        return *this;
    }

    ~ScopedRange()
    {
        end();
    }

private:
    void end()
    {
#ifdef ENABLE_NVTX
        if (id_ != 0) {
            nvtxRangeEnd(id_);
            id_ = 0;
        }
#endif
    }

#ifdef ENABLE_NVTX
    nvtxRangeId_t id_ = 0;
#else
    std::uint64_t id_ = 0;
#endif
    std::string owned_message_;
};

namespace color {
constexpr std::uint32_t Decode = 0xFF4CAF50;
constexpr std::uint32_t Preprocess = 0xFF42A5F5;
constexpr std::uint32_t Inference = 0xFFFFA726;
constexpr std::uint32_t Postprocess = 0xFFAB47BC;
constexpr std::uint32_t Control = 0xFF26A69A;
}

inline std::string makeStageLabel(const char* stage, int channelId)
{
    return std::string(stage) + "[ch=" + std::to_string(channelId) + "]";
}

inline std::string makeWorkerLabel(const char* stage, int workerId, std::size_t batchSize)
{
    return std::string(stage) + "[worker=" + std::to_string(workerId)
        + ",batch=" + std::to_string(batchSize) + "]";
}

} // namespace nvtxutil