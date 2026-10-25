/**
 * @file rtcp.cc
 * @brief RTCP Sender Report 实现
 */

#include "rtcp.h"

#include <chrono>
#include <cstring>

namespace rtp_packet
{

int RtcpSrBuilder::buildSr(uint8_t* buf, uint32_t ssrc, uint32_t ntp_sec, uint32_t ntp_frac, uint32_t rtp_timestamp,
                           const RtcpSrStats& stats)
{
    if (!buf)
    {
        return -1;
    }

    size_t offset = 0;

    // Byte 0: V=2, P=0, RC=0
    // V(2) = 10, P(1) = 0, RC(5) = 00000
    buf[offset++] = 0x80;  // V=2, P=0, RC=0

    // Byte 1: PT=200 (SR)
    buf[offset++] = kRtcpPtSR;

    // Byte 2-3: Length = 6 (in 32-bit words minus 1)
    // SR 固定 28 字节 = 7 个 32-bit word, length = 7 - 1 = 6
    buf[offset++] = 0x00;
    buf[offset++] = 0x06;

    // Byte 4-7: SSRC
    buf[offset++] = (ssrc >> 24) & 0xFF;
    buf[offset++] = (ssrc >> 16) & 0xFF;
    buf[offset++] = (ssrc >> 8) & 0xFF;
    buf[offset++] = ssrc & 0xFF;

    // Byte 8-11: NTP timestamp (most significant word)
    buf[offset++] = (ntp_sec >> 24) & 0xFF;
    buf[offset++] = (ntp_sec >> 16) & 0xFF;
    buf[offset++] = (ntp_sec >> 8) & 0xFF;
    buf[offset++] = ntp_sec & 0xFF;

    // Byte 12-15: NTP timestamp (least significant word)
    buf[offset++] = (ntp_frac >> 24) & 0xFF;
    buf[offset++] = (ntp_frac >> 16) & 0xFF;
    buf[offset++] = (ntp_frac >> 8) & 0xFF;
    buf[offset++] = ntp_frac & 0xFF;

    // Byte 16-19: RTP timestamp
    buf[offset++] = (rtp_timestamp >> 24) & 0xFF;
    buf[offset++] = (rtp_timestamp >> 16) & 0xFF;
    buf[offset++] = (rtp_timestamp >> 8) & 0xFF;
    buf[offset++] = rtp_timestamp & 0xFF;

    // Byte 20-23: Sender's packet count
    buf[offset++] = (stats.packet_count >> 24) & 0xFF;
    buf[offset++] = (stats.packet_count >> 16) & 0xFF;
    buf[offset++] = (stats.packet_count >> 8) & 0xFF;
    buf[offset++] = stats.packet_count & 0xFF;

    // Byte 24-27: Sender's octet count
    buf[offset++] = (stats.octet_count >> 24) & 0xFF;
    buf[offset++] = (stats.octet_count >> 16) & 0xFF;
    buf[offset++] = (stats.octet_count >> 8) & 0xFF;
    buf[offset++] = stats.octet_count & 0xFF;

    return static_cast<int>(offset);  // 应返回 28
}

void RtcpSrBuilder::getNtpTimestamp(uint32_t& ntp_sec, uint32_t& ntp_frac)
{
    // NTP 时间戳：从 1900-01-01 00:00:00 开始的秒数
    // Unix 时间戳从 1970-01-01 开始，需要加上 1900-1970 的秒数差
    // 1900-1970 = 70 年，其中 17 个闰年
    // 70 * 365 + 17 = 25567 天
    static constexpr uint32_t kNtpUnixEpochDiff = 2208988800UL;  // 25567 * 86400

    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count() % 1000000;

    ntp_sec = static_cast<uint32_t>(time_t_now) + kNtpUnixEpochDiff;
    // 将微秒转换为 NTP 小数部分 (1/2^32 秒)
    ntp_frac = static_cast<uint32_t>((us * 4294.967296));  // us * 2^32 / 10^6
}

}  // namespace rtp_packet
