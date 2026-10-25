/**
 * @file rtcp.h
 * @brief RTCP Sender Report 生成器
 *
 * 实现 RTCP SR (Sender Report) 包的构造和序列化。
 *
 * RTCP SR 格式 (RFC 3550):
 *   0                   1                   2                   3
 *   0 1 2 3 4 5 6 7|0 1 2 3 4 5 6 7|0 1 2 3 4 5 6 7|0 1 2 3 4 5 6 7
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |V=2|P|    RC   |   PT=SR=200   |             length            |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |                         SSRC of sender                        |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |                  NTP timestamp, most significant word          |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |                  NTP timestamp, least significant word         |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |                         RTP timestamp                         |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |                     sender's packet count                     |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |                      sender's octet count                     |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */

#ifndef RTP_PACKET_RTCP_H_
#define RTP_PACKET_RTCP_H_

#include <cstddef>
#include <cstdint>

namespace rtp_packet
{

/** RTCP SR 包固定长度（字节）：Header(4) + SSRC(4) + NTP(8) + RTP_TS(4) + PKT_CNT(4) + OCT_CNT(4) = 28 */
static constexpr size_t kRtcpSrSize = 28;

/** RTCP SR Payload Type */
static constexpr uint8_t kRtcpPtSR = 200;

/**
 * @brief RTCP Sender Report 统计信息
 */
struct RtcpSrStats
{
    uint32_t packet_count; /**< 已发送的 RTP 包数 */
    uint32_t octet_count;  /**< 已发送的 RTP 字节数（payload only） */

    RtcpSrStats() : packet_count(0), octet_count(0) {}
};

/**
 * @brief RTCP Sender Report 构造器
 *
 * 线程安全：非线程安全，调用者需保证同步。
 */
class RtcpSrBuilder
{
public:
    /**
     * @brief 构造 RTCP SR 包
     *
     * @param[out] buf 输出缓冲区，至少 kRtcpSrSize (28) 字节
     * @param[in] ssrc SSRC
     * @param[in] ntp_sec NTP 时间戳秒部分
     * @param[in] ntp_frac NTP 时间戳小数部分
     * @param[in] rtp_timestamp RTP 时间戳
     * @param[in] stats 发送统计
     * @return 写入的字节数（固定 kRtcpSrSize），<0 表示错误
     */
    static int buildSr(uint8_t* buf, uint32_t ssrc, uint32_t ntp_sec, uint32_t ntp_frac, uint32_t rtp_timestamp,
                       const RtcpSrStats& stats);

    /**
     * @brief 获取当前 NTP 时间戳
     *
     * @param[out] ntp_sec NTP 秒部分
     * @param[out] ntp_frac NTP 小数部分
     */
    static void getNtpTimestamp(uint32_t& ntp_sec, uint32_t& ntp_frac);
};

}  // namespace rtp_packet

#endif  // RTP_PACKET_RTCP_H_
