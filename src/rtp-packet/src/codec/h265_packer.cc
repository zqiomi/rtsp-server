#include "h265_packer.h"

#include <cstring>

#include "nalu_splitter.h"
#include "rtcp/rtcp.h"
#include "rtp/rtp_header.h"
#include "util/constants.h"
#include "log/module_log.h"

namespace rtp_packet
{

H265Packer::H265Packer(const PackerConfig& config) : RtpPackerBase(config) {}

int H265Packer::packFrame(const uint8_t* frame, size_t len, uint32_t timestamp)
{
    if (!frame || len == 0)
    {
        return -1;
    }

    // 计算 NALU 数量
    size_t temp_pos = 0;
    NaluInfo info;
    size_t nalu_count = 0;
    while (findNextNalu(frame, len, temp_pos, info))
    {
        nalu_count++;
    }

    if (nalu_count == 0)
    {
        LOG_WARN("No NALU found in frame");
        return -1;
    }

    // 打包每个 NALU
    size_t pos = 0;
    size_t nalu_index = 0;
    while (findNextNalu(frame, len, pos, info))
    {
        nalu_index++;
        bool is_last_nalu = (nalu_index == nalu_count);
        int ret = packNalu(info.data, info.size, timestamp, is_last_nalu);
        if (ret < 0)
        {
            LOG_ERROR("Failed to pack NALU %zu", nalu_index);
            return ret;
        }
    }

    return 0;
}

int H265Packer::packNalu(const uint8_t* nalu, size_t nalu_len, uint32_t timestamp, bool is_last_nalu)
{
    if (nalu_len < 2)
    {
        return -1;
    }

    // H.265 NALU Header 为 2 字节
    // Single NAL Unit: RTP Header (12) + NALU Header (2) + payload
    // FU: RTP Header (12) + Payload Header (2) + FU Header (1) + payload

    size_t max_payload_size = mtu_ - kRtpFixedHeaderSize;

    if (nalu_len <= max_payload_size)
    {
        // Single NAL Unit Packet
        return packSingleNalu(nalu, nalu_len, timestamp, is_last_nalu);
    }
    else
    {
        // FU 分片
        return packFu(nalu, nalu_len, timestamp, is_last_nalu);
    }
}

int H265Packer::packSingleNalu(const uint8_t* nalu, size_t nalu_len, uint32_t timestamp, bool marker)
{
    // 构造 RTP 包：RTP Header + NALU (含 2 字节 NALU Header)
    uint8_t packet[kMaxRtpPacketSize];
    RtpHeader header;
    header.payload_type = payload_type_;
    header.marker = marker;
    header.sequence = sequence_++;
    header.timestamp = timestamp;
    header.ssrc = ssrc_;

    size_t offset = serializeRtpHeader(packet, header);

    // 直接拷贝 NALU（包含 2 字节 NALU Header）
    std::memcpy(packet + offset, nalu, nalu_len);
    offset += nalu_len;

    // 更新统计
    updateStats(nalu_len);

    sendPacket(packet, offset, marker, timestamp);
    return 0;
}

int H265Packer::packFu(const uint8_t* nalu, size_t nalu_len, uint32_t timestamp, bool is_last_nalu)
{
    // H.265 FU 格式 (RFC 7798)：
    // | RTP Header (12) | Payload Header (2) | FU Header (1) | FU Payload |
    //
    // Payload Header:
    //   Byte 0: forbidden_zero_bit(1) | Type=49(6) | nuh_layer_id(1)
    //   Byte 1: nuh_layer_id(5) | nuh_temporal_id_plus1(3)
    //
    // FU Header: S(1) | E(1) | FuType(6)

    if (nalu_len < 2)
    {
        return -1;
    }

    // 原始 NALU Header (2 字节)
    uint8_t nalu_header_0 = nalu[0];
    uint8_t nalu_header_1 = nalu[1];

    // 提取原始 NALU 类型
    uint8_t nalu_type = getH265NaluType(nalu);

    // 构造 Payload Header (2 字节)
    // Type = 49 (FU)
    uint8_t payload_header_0 = (nalu_header_0 & 0x81) | (kH265NalTypeFU << 1);
    uint8_t payload_header_1 = nalu_header_1;

    // 每个 FU 包的 payload 大小 = MTU - RTP Header (12) - Payload Header (2) - FU Header (1)
    size_t fu_payload_size = mtu_ - kRtpFixedHeaderSize - 3;

    size_t offset = 2;  // 跳过原始 2 字节 NALU Header
    bool is_first = true;

    while (offset < nalu_len)
    {
        size_t chunk_size = nalu_len - offset;
        if (chunk_size > fu_payload_size)
        {
            chunk_size = fu_payload_size;
        }

        bool is_last = (offset + chunk_size >= nalu_len);

        // FU Header
        uint8_t fu_header = nalu_type;
        if (is_first)
        {
            fu_header |= 0x80;  // S bit
            is_first = false;
        }
        if (is_last)
        {
            fu_header |= 0x40;  // E bit
        }

        // 构造 RTP 包
        uint8_t packet[kMaxRtpPacketSize];
        RtpHeader header;
        header.payload_type = payload_type_;
        header.marker = is_last && is_last_nalu;  // 只有最后一个分片的最后一个包才设置 M bit
        header.sequence = sequence_++;
        header.timestamp = timestamp;
        header.ssrc = ssrc_;

        size_t pkt_offset = serializeRtpHeader(packet, header);

        // Payload Header (2 字节)
        packet[pkt_offset++] = payload_header_0;
        packet[pkt_offset++] = payload_header_1;

        // FU Header (1 字节)
        packet[pkt_offset++] = fu_header;

        // Payload
        std::memcpy(packet + pkt_offset, nalu + offset, chunk_size);
        pkt_offset += chunk_size;

        sendPacket(packet, pkt_offset, header.marker, timestamp);

        offset += chunk_size;
    }

    // 更新统计（只计算原始 NALU 的 payload）
    updateStats(nalu_len - 2);

    return 0;
}

void H265Packer::sendPacket(const uint8_t* data, size_t len, bool marker, uint32_t timestamp)
{
    (void)marker;
    (void)timestamp;
    if (!callback_)
    {
        return;
    }
    callback_(data, len, user_data_);
}

void H265Packer::setSSRC(uint32_t ssrc)
{
    ssrc_ = ssrc;
}

void H265Packer::setMTU(size_t mtu)
{
    mtu_ = mtu;
}

uint32_t H265Packer::getSSRC() const
{
    return ssrc_;
}

size_t H265Packer::getMTU() const
{
    return mtu_;
}

RtcpSrStats H265Packer::getStats() const
{
    return stats_;
}

int H265Packer::buildRtcpSr(uint8_t* buf, uint32_t rtp_timestamp)
{
    if (!buf)
    {
        return -1;
    }

    uint32_t ntp_sec, ntp_frac;
    RtcpSrBuilder::getNtpTimestamp(ntp_sec, ntp_frac);

    return RtcpSrBuilder::buildSr(buf, ssrc_, ntp_sec, ntp_frac, rtp_timestamp, stats_);
}

}  // namespace rtp_packet
