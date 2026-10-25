/**
 * @file packer_base.h
 * @brief RTP 打包器基类定义
 */

#ifndef RTP_PACKET_PACKER_BASE_H_
#define RTP_PACKET_PACKER_BASE_H_

#include <cstddef>
#include <cstdint>

#include "rtcp/rtcp.h"
#include "rtp_packet.h"

namespace rtp_packet
{

/**
 * @brief RTP 打包回调函数类型（内部使用）
 *
 * @param data RTP 包数据
 * @param len RTP 包长度
 * @param user_data 用户自定义数据
 */
typedef void (*PackerCallback)(const uint8_t* data, size_t len, void* user_data);

/**
 * @brief RTP 打包器配置（内部使用）
 */
struct PackerConfig
{
    RtpCodecType codec;         /**< 编解码类型，默认 RTP_CODEC_H264 */
    uint32_t ssrc;              /**< SSRC，默认 0 */
    uint16_t seq_start;         /**< 起始序列号，默认 0（随机） */
    size_t mtu;                 /**< 最大传输单元（字节），默认 1400 */
    uint8_t payload_type;       /**< RTP Payload Type，默认 96 */
    RtpPacketCallback callback; /**< 数据回调接口 */
    void* user_data;            /* 用户数据 */
    bool auto_sps_pps;          /**< 自动在 IDR 帧前插入 SPS/PPS，默认 true */
    PackerConfig()
        : codec(RTP_CODEC_H264),
          ssrc(0),
          seq_start(0),
          mtu(1400),
          payload_type(96),
          callback(nullptr),
          user_data(nullptr),
          auto_sps_pps(true)
    {
    }
};

/**
 * @brief RTP 打包器虚基类
 *
 * 所有编解码格式的打包器都继承此处，实现 packFrame 方法。
 */
class RtpPackerBase
{
public:
    RtpPackerBase(const PackerConfig& config)
        : ssrc_(config.ssrc),
          sequence_(config.seq_start),
          mtu_(config.mtu),
          payload_type_(config.payload_type),
          callback_(config.callback),
          user_data_(config.user_data),
          auto_sps_pps_(config.auto_sps_pps)
    {
    }
    virtual ~RtpPackerBase() = default;

    /**
     * @brief 打包一帧数据
     *
     * @param frame Annex-B 格式的帧数据
     * @param len 帧数据长度
     * @param timestamp RTP 时间戳
     * @return 0 成功，<0 失败
     */
    virtual int packFrame(const uint8_t* frame, size_t len, uint32_t timestamp) = 0;

    /**
     * @brief 设置 SSRC
     */
    virtual void setSSRC(uint32_t ssrc) = 0;

    /**
     * @brief 设置 MTU
     */
    virtual void setMTU(size_t mtu) = 0;

    /**
     * @brief 获取 SSRC
     */
    virtual uint32_t getSSRC() const = 0;

    /**
     * @brief 获取 MTU
     */
    virtual size_t getMTU() const = 0;

    /**
     * @brief 获取发送统计
     */
    virtual RtcpSrStats getStats() const = 0;

    /**
     * @brief 构造 RTCP Sender Report
     *
     * @param[out] buf 输出缓冲区，至少 28 字节
     * @param[in] rtp_timestamp 当前 RTP 时间戳
     * @return SR 包长度（28），<0 表示错误
     */
    virtual int buildRtcpSr(uint8_t* buf, uint32_t rtp_timestamp) = 0;

    /**
     * @brief 设置是否自动插入 SPS/PPS
     */
    void setAutoSpsPps(bool enable)
    {
        auto_sps_pps_ = enable;
    }

    /**
     * @brief 获取自动 SPS/PPS 设置
     */
    bool getAutoSpsPps() const
    {
        return auto_sps_pps_;
    }

protected:
    /**
     * @brief 更新发送统计
     */
    void updateStats(size_t payload_len)
    {
        stats_.packet_count++;
        stats_.octet_count += static_cast<uint32_t>(payload_len);
    }

    uint32_t ssrc_;
    uint16_t sequence_;
    size_t mtu_;
    uint8_t payload_type_;
    PackerCallback callback_;
    void* user_data_;
    bool auto_sps_pps_;
    RtcpSrStats stats_;
};

}  // namespace rtp_packet

#endif  // RTP_PACKET_PACKER_BASE_H_
/**
 * @file packer_base.h
 * @brief RTP 打包器基类定义
 */

#ifndef RTP_PACKET_PACKER_BASE_H_
#define RTP_PACKET_PACKER_BASE_H_

#include <cstddef>
#include <cstdint>

#include "rtp_packet.h"

namespace rtp_packet
{

/**
 * @brief RTP 打包回调函数类型（内部使用）
 *
 * @param data RTP 包数据
 * @param len RTP 包长度
 * @param user_data 用户自定义数据
 */
typedef void (*PackerCallback)(const uint8_t* data, size_t len, void* user_data);

/**
 * @brief RTP 打包器配置（内部使用）
 */
struct PackerConfig
{
    RtpCodecType codec;         /**< 编解码类型，默认 RTP_CODEC_H264 */
    uint32_t ssrc;              /**< SSRC，默认 0 */
    uint16_t seq_start;         /**< 起始序列号，默认 0（随机） */
    size_t mtu;                 /**< 最大传输单元（字节），默认 1400 */
    uint8_t payload_type;       /**< RTP Payload Type，默认 96 */
    RtpPacketCallback callback; /**< 数据回调接口 */
    void* user_data;            /* 用户数据 */
    PackerConfig() : ssrc(0), seq_start(0), mtu(1400), payload_type(96), callback(nullptr), user_data(nullptr) {}
};

/**
 * @brief RTP 打包器虚基类
 *
 * 所有编解码格式的打包器都继承此类，实现 packFrame 方法。
 */
class RtpPackerBase
{
public:
    RtpPackerBase(const PackerConfig& config)
        : ssrc_(config.ssrc),
          sequence_(config.seq_start),
          mtu_(config.mtu),
          payload_type_(config.payload_type),
          callback_(config.callback),
          user_data_(config.user_data)
    {
    }
    virtual ~RtpPackerBase() = default;

    /**
     * @brief 打包一帧数据
     *
     * @param frame Annex-B 格式的帧数据
     * @param len 帧数据长度
     * @param timestamp RTP 时间戳
     * @param callback 打包结果回调
     * @param user_data 回调用户数据
     * @return 0 成功，<0 失败
     */
    virtual int packFrame(const uint8_t* frame, size_t len, uint32_t timestamp) = 0;

    /**
     * @brief 设置 SSRC
     */
    virtual void setSSRC(uint32_t ssrc) = 0;

    /**
     * @brief 设置 MTU
     */
    virtual void setMTU(size_t mtu) = 0;

    /**
     * @brief 获取 SSRC
     */
    virtual uint32_t getSSRC() const = 0;

    /**
     * @brief 获取 MTU
     */
    virtual size_t getMTU() const = 0;

protected:
    uint32_t ssrc_;
    uint16_t sequence_;
    size_t mtu_;
    uint8_t payload_type_;
    PackerCallback callback_;
    void* user_data_;
};

}  // namespace rtp_packet

#endif  // RTP_PACKET_PACKER_BASE_H_
