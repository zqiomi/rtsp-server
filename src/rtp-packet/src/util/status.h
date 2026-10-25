#ifndef RTP_PACKET_STATUS_H_
#define RTP_PACKET_STATUS_H_

#include <string>

namespace rtp_packet
{

// 状态码枚举
enum class StatusCode
{
    kOk = 0,
    kError = -1,
    kInvalidArgument = -2,
    kBufferTooSmall = -3,
    kParseError = -4,
    kNotImplemented = -5,
    kOutOfMemory = -6,
};

// Status 类 - 用于错误处理
class Status
{
public:
    // 默认构造 - OK
    Status() : code_(StatusCode::kOk) {}

    // 从状态码构造
    explicit Status(StatusCode code) : code_(code) {}

    // 从状态码和错误信息构造
    Status(StatusCode code, const std::string& message) : code_(code), message_(message) {}

    // 拷贝构造
    Status(const Status& other) = default;

    // 移动构造
    Status(Status&& other) noexcept : code_(other.code_), message_(std::move(other.message_)) {}

    // 拷贝赋值
    Status& operator=(const Status& other) = default;

    // 移动赋值
    Status& operator=(Status&& other) noexcept
    {
        if (this != &other)
        {
            code_ = other.code_;
            message_ = std::move(other.message_);
        }
        return *this;
    }

    // 是否成功
    bool ok() const
    {
        return code_ == StatusCode::kOk;
    }

    // 获取状态码
    StatusCode code() const
    {
        return code_;
    }

    // 获取错误信息
    const std::string& message() const
    {
        return message_;
    }

    // 设置错误信息
    void set_message(const std::string& message)
    {
        message_ = message;
    }

    // 转换为字符串
    std::string ToString() const
    {
        switch (code_)
        {
            case StatusCode::kOk:
                return "OK";
            case StatusCode::kError:
                return "Error: " + message_;
            case StatusCode::kInvalidArgument:
                return "InvalidArgument: " + message_;
            case StatusCode::kBufferTooSmall:
                return "BufferTooSmall: " + message_;
            case StatusCode::kParseError:
                return "ParseError: " + message_;
            case StatusCode::kNotImplemented:
                return "NotImplemented: " + message_;
            case StatusCode::kOutOfMemory:
                return "OutOfMemory: " + message_;
            default:
                return "Unknown";
        }
    }

    // 静态工厂方法
    static Status Ok()
    {
        return Status();
    }
    static Status Error(const std::string& message = "")
    {
        return Status(StatusCode::kError, message);
    }
    static Status InvalidArgument(const std::string& message = "")
    {
        return Status(StatusCode::kInvalidArgument, message);
    }
    static Status BufferTooSmall(const std::string& message = "")
    {
        return Status(StatusCode::kBufferTooSmall, message);
    }
    static Status ParseError(const std::string& message = "")
    {
        return Status(StatusCode::kParseError, message);
    }
    static Status NotImplemented(const std::string& message = "")
    {
        return Status(StatusCode::kNotImplemented, message);
    }
    static Status OutOfMemory(const std::string& message = "")
    {
        return Status(StatusCode::kOutOfMemory, message);
    }

private:
    StatusCode code_;
    std::string message_;
};

}  // namespace rtp_packet

#endif  // RTP_PACKET_STATUS_H_
