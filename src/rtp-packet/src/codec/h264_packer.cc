#include "h264_packer.h"

#include <cstring>

#include "codec/packer_base.h"
#include "nalu_splitter.h"
#include "rtcp/rtcp.h"
#include "rtp/rtp_header.h"
#include "util/constants.h"
#include "log/module_log.h"

namespace rtp_packet
{

// H.264 NALU 类型常量
static constexpr uint8_t kH264NalTypeSPS = 7;
static constexpr uint8_t kH264NalTypePPS = 8;
static constexpr uint8_t kH264NalTypeIDR = 5;

H264Packer::H264Packer(const PackerConfig& config) : RtpPackerBase(config) {}

int H264Packer::packFrame(const uint8_t* frame, size_t len, uint32_t timestamp)
{
    if (!frame || len == 0)
    {
        return -1;
    }

    // 收集所有 NALU，以便判断哪个是最后一个
    // 先计算 NALU 数量
    size_t pos = 0;
    NaluInfo info;
    size_t nalu_count = 0;
    size_t temp_pos = 0;
    while (findNextNalu(frame, len, temp_pos, info))
    {
        nalu_count++;
    }

    if (nalu_count == 0)
    {
        LOG_WARN("No NALU found in frame");
        return -1;
    }

    // 检查帧中是否包含 IDR，如果是且启用了 auto_sps_pps_，需要先发送缓存的 SPS/PPS
    bool has_idr = false;
    bool has_sps = false;
    bool has_pps = false;

    temp_pos = 0;
    while (findNextNalu(frame, len, temp_pos, info))
    {
        uint8_t nalu_type = getH264NaluType(info.data[0]);
        if (nalu_type == kH264NalTypeIDR)
        {
            has_idr = true;
        }
        else if (nalu_type == kH264NalTypeSPS)
        {
            has_sps = true;
        }
        else if (nalu_type == kH264NalTypePPS)
        {
            has_pps = true;
        }
    }

    // 如果是 IDR 帧且没有自带 SPS/PPS，且启用了自动插入，则发送缓存的 SPS/PPS
    if (has_idr && !has_sps && !has_pps && auto_sps_pps_ && !cached_sps_.empty() && !cached_pps_.empty())
    {
        int ret = sendCachedSpsPps(timestamp);
        if (ret < 0)
        {
            LOG_WARN("Failed to send cached SPS/PPS");
        }
    }

    // 打包每个 NALU
    pos = 0;
    size_t nalu_index = 0;
    while (findNextNalu(frame, len, pos, info))
    {
        nalu_index++;
        bool is_last_nalu = (nalu_index == nalu_count);
        uint8_t nalu_type = getH264NaluType(info.data[0]);

        // 缓存 SPS/PPS
        if (nalu_type == kH264NalTypeSPS || nalu_type == kH264NalTypePPS)
        {
            cacheSpsPps(info.data, info.size, nalu_type);
        }

        int ret = packNalu(info.data, info.size, timestamp, is_last_nalu);
        if (ret < 0)
        {
            LOG_ERROR("Failed to pack NALU %zu", nalu_index);
            return ret;
        }
    }

    return 0;
}

int H264Packer::packNalu(const uint8_t* nalu, size_t nalu_len, uint32_t timestamp, bool is_last_nalu)
{
    if (nalu_len == 0)
    {
        return -1;
    }

    // RTP 头部占 12 字节
    // Single NAL Unit: RTP Header (12) + NALU Header (1) + payload
    // FU-A: RTP Header (12) + FU Indicator (1) + FU Header (1) + payload

    size_t max_payload_size = mtu_ - kRtpFixedHeaderSize;

    if (nalu_len <= max_payload_size)
    {
        // Single NAL Unit Packet
        return packSingleNalu(nalu, nalu_len, timestamp, is_last_nalu);
    }
    else
    {
        // FU-A 分片
        return packFuA(nalu, nalu_len, timestamp, is_last_nalu);
    }
}

int H264Packer::packSingleNalu(const uint8_t* nalu, size_t nalu_len, uint32_t timestamp, bool marker)
{
    // 构造 RTP 包：RTP Header + NALU
    uint8_t packet[kMaxRtpPacketSize];
    RtpHeader header;
    header.payload_type = payload_type_;
    header.marker = marker;
    header.sequence = sequence_++;
    header.timestamp = timestamp;
    header.ssrc = ssrc_;

    size_t offset = serializeRtpHeader(packet, header);

    // 直接拷贝 NALU（包含 NALU Header）
    std::memcpy(packet + offset, nalu, nalu_len);
    offset += nalu_len;

    // 更新统计
    updateStats(nalu_len);

    sendPacket(packet, offset, marker, timestamp);
    return 0;
}

int H264Packer::packFuA(const uint8_t* nalu, size_t nalu_len, uint32_t timestamp, bool is_last_nalu)
{
    // H.264 FU-A 格式：
    // | RTP Header (12) | FU Indicator (1) | FU Header (1) | FU Payload |
    //
    // FU Indicator: F(1) | NRI(2) | Type=28(5)
    // FU Header: S(1) | E(1) | R(1) | Type(5)

    if (nalu_len < 1)
    {
        return -1;
    }

    uint8_t nalu_header = nalu[0];
    uint8_t nri = (nalu_header >> 5) & 0x03;
    uint8_t nalu_type = nalu_header & 0x1F;

    // FU Indicator
    uint8_t fu_indicator = (nalu_header & 0x80) | (nri << 5) | kH264NalTypeFU_A;

    // 每个 FU-A 包的 payload 大小 = MTU - RTP Header (12) - FU Indicator (1) - FU Header (1)
    size_t fu_payload_size = mtu_ - kRtpFixedHeaderSize - 2;

    size_t offset = 1;  // 跳过原始 NALU Header
    bool is_first = true;
    size_t total_payload = 0;

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

        // FU Indicator
        packet[pkt_offset++] = fu_indicator;

        // FU Header
        packet[pkt_offset++] = fu_header;

        // Payload
        std::memcpy(packet + pkt_offset, nalu + offset, chunk_size);
        pkt_offset += chunk_size;

        total_payload += chunk_size;

        sendPacket(packet, pkt_offset, header.marker, timestamp);

        offset += chunk_size;
    }

    // 更新统计（只计算原始 NALU 的 payload，不含 FU 头）
    updateStats(nalu_len - 1);

    return 0;
}

void H264Packer::sendPacket(const uint8_t* data, size_t len, bool marker, uint32_t timestamp)
{
    (void)marker;
    (void)timestamp;
    if (!callback_)
    {
        return;
    }
    callback_(data, len, user_data_);
}

void H264Packer::cacheSpsPps(const uint8_t* nalu, size_t nalu_len, uint8_t nalu_type)
{
    if (nalu_type == kH264NalTypeSPS)
    {
        cached_sps_.assign(nalu, nalu + nalu_len);
        LOG_DEBUG("Cached SPS, size=%zu", nalu_len);
    }
    else if (nalu_type == kH264NalTypePPS)
    {
        cached_pps_.assign(nalu, nalu + nalu_len);
        LOG_DEBUG("Cached PPS, size=%zu", nalu_len);
    }
}

int H264Packer::sendCachedSpsPps(uint32_t timestamp)
{
    if (cached_sps_.empty() || cached_pps_.empty())
    {
        return -1;
    }

    LOG_DEBUG("Sending cached SPS(%zu) + PPS(%zu)", cached_sps_.size(), cached_pps_.size());

    // 发送 SPS（不是最后一个）
    int ret = packNalu(cached_sps_.data(), cached_sps_.size(), timestamp, false);
    if (ret < 0)
    {
        return ret;
    }

    // 发送 PPS（不是最后一个，后面还有 IDR）
    ret = packNalu(cached_pps_.data(), cached_pps_.size(), timestamp, false);
    if (ret < 0)
    {
        return ret;
    }

    return 0;
}

RtcpSrStats H264Packer::getStats() const
{
    return stats_;
}

int H264Packer::buildRtcpSr(uint8_t* buf, uint32_t rtp_timestamp)
{
    if (!buf)
    {
        return -1;
    }

    uint32_t ntp_sec, ntp_frac;
    RtcpSrBuilder::getNtpTimestamp(ntp_sec, ntp_frac);

    return RtcpSrBuilder::buildSr(buf, ssrc_, ntp_sec, ntp_frac, rtp_timestamp, stats_);
}

void H264Packer::setSSRC(uint32_t ssrc)
{
    ssrc_ = ssrc;
}

void H264Packer::setMTU(size_t mtu)
{
    mtu_ = mtu;
}

uint32_t H264Packer::getSSRC() const
{
    return ssrc_;
}

size_t H264Packer::getMTU() const
{
    return mtu_;
}

}  // namespace rtp_packet
