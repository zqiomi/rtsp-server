/**
 * @file rtp_header.h
 * @brief RTP 头部构造与序列化
 *
 * RTP 固定头部 12 字节：
 *   0                   1                   2                   3
 *   0 1 2 3 4 5 6 7|0 1 2 3 4 5 6 7|0 1 2 3 4 5 6 7|0 1 2 3 4 5 6 7
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |V=2|P|X|  CC   |M|     PT      |       sequence number         |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |                           timestamp                           |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |           synchronization source (SSRC) identifier            |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */

#ifndef RTP_PACKET_RTP_HEADER_H_
#define RTP_PACKET_RTP_HEADER_H_

#include <cstddef>
#include <cstdint>

namespace rtp_packet
{

/** RTP 固定头部长度（字节） */
static constexpr size_t kRtpFixedHeaderSize = 12;

/** RTP 版本号（固定为 2） */
static constexpr uint8_t kRtpVersion = 2;

/**
 * @brief RTP 头部结构体
 *
 * 只包含固定头部字段，不含 CSRC、扩展头、Padding。
 */
struct RtpHeader
{
    uint8_t payload_type; /**< Payload Type (7 bit) */
    bool marker;          /**< Marker bit */
    uint16_t sequence;    /**< Sequence Number */
    uint32_t timestamp;   /**< Timestamp */
    uint32_t ssrc;        /**< SSRC */

    RtpHeader() : payload_type(96), marker(false), sequence(0), timestamp(0), ssrc(0) {}
};

/**
 * @brief 将 RTP 头部序列化到 buffer
 *
 * @param[out] buf 输出 buffer，至少 kRtpFixedHeaderSize (12) 字节
 * @param[in] header RTP 头部
 * @return 写入的字节数（固定 12）
 */
size_t serializeRtpHeader(uint8_t* buf, const RtpHeader& header);

/**
 * @brief 从 buffer 反序列化 RTP 头部
 *
 * @param[in] buf 输入 buffer，至少 kRtpFixedHeaderSize (12) 字节
 * @param[in] len buffer 长度
 * @param[out] header 输出 RTP 头部
 * @return 读取的字节数，< 0 表示错误
 */
int deserializeRtpHeader(const uint8_t* buf, size_t len, RtpHeader& header);

}  // namespace rtp_packet

#endif  // RTP_PACKET_RTP_HEADER_H_
