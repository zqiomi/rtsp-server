#include "rtp_header.h"

#include <cstring>

namespace rtp_packet
{

size_t serializeRtpHeader(uint8_t* buf, const RtpHeader& header)
{
    // Byte 0: V=2, P=0, X=0, CC=0
    buf[0] = (kRtpVersion << 6);  // 0x80

    // Byte 1: M + PT
    buf[1] = header.payload_type & 0x7F;
    if (header.marker)
    {
        buf[1] |= 0x80;
    }

    // Byte 2-3: Sequence Number (big-endian)
    buf[2] = static_cast<uint8_t>((header.sequence >> 8) & 0xFF);
    buf[3] = static_cast<uint8_t>(header.sequence & 0xFF);

    // Byte 4-7: Timestamp (big-endian)
    buf[4] = static_cast<uint8_t>((header.timestamp >> 24) & 0xFF);
    buf[5] = static_cast<uint8_t>((header.timestamp >> 16) & 0xFF);
    buf[6] = static_cast<uint8_t>((header.timestamp >> 8) & 0xFF);
    buf[7] = static_cast<uint8_t>(header.timestamp & 0xFF);

    // Byte 8-11: SSRC (big-endian)
    buf[8] = static_cast<uint8_t>((header.ssrc >> 24) & 0xFF);
    buf[9] = static_cast<uint8_t>((header.ssrc >> 16) & 0xFF);
    buf[10] = static_cast<uint8_t>((header.ssrc >> 8) & 0xFF);
    buf[11] = static_cast<uint8_t>(header.ssrc & 0xFF);

    return kRtpFixedHeaderSize;
}

int deserializeRtpHeader(const uint8_t* buf, size_t len, RtpHeader& header)
{
    if (len < kRtpFixedHeaderSize)
    {
        return -1;
    }

    // Byte 0: V, P, X, CC
    uint8_t version = (buf[0] >> 6) & 0x03;
    if (version != kRtpVersion)
    {
        return -1;  // 不支持的版本
    }

    // Byte 1: M + PT
    header.marker = (buf[1] & 0x80) != 0;
    header.payload_type = buf[1] & 0x7F;

    // Byte 2-3: Sequence Number
    header.sequence = static_cast<uint16_t>((buf[2] << 8) | buf[3]);

    // Byte 4-7: Timestamp
    header.timestamp = (static_cast<uint32_t>(buf[4]) << 24) | (static_cast<uint32_t>(buf[5]) << 16) |
                       (static_cast<uint32_t>(buf[6]) << 8) | static_cast<uint32_t>(buf[7]);

    // Byte 8-11: SSRC
    header.ssrc = (static_cast<uint32_t>(buf[8]) << 24) | (static_cast<uint32_t>(buf[9]) << 16) |
                  (static_cast<uint32_t>(buf[10]) << 8) | static_cast<uint32_t>(buf[11]);

    return static_cast<int>(kRtpFixedHeaderSize);
}

}  // namespace rtp_packet
