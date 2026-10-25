/**
 * @file h265_packer.h
 * @brief H.265 RTP 打包器 (RFC 7798)
 *
 * 支持：
 * - Single NAL Unit Packet：NALU 长度 <= MTU 时单包发送
 * - FU 分片：NALU 长度 > MTU 时按 MTU 分片
 */

#ifndef RTP_PACKET_H265_PACKER_H_
#define RTP_PACKET_H265_PACKER_H_

#include "packer_base.h"

namespace rtp_packet
{

/**
 * @brief H.265 RTP 打包器
 */
class H265Packer : public RtpPackerBase
{
public:
    H265Packer(const PackerConfig& config);
    ~H265Packer() override = default;

    int packFrame(const uint8_t* frame, size_t len, uint32_t timestamp) override;
    void setSSRC(uint32_t ssrc) override;
    void setMTU(size_t mtu) override;
    uint32_t getSSRC() const override;
    size_t getMTU() const override;
    RtcpSrStats getStats() const override;
    int buildRtcpSr(uint8_t* buf, uint32_t rtp_timestamp) override;

private:
    /**
     * @brief 打包单个 NALU
     */
    int packNalu(const uint8_t* nalu, size_t nalu_len, uint32_t timestamp, bool is_last_nalu);

    /**
     * @brief 打包 Single NAL Unit Packet
     */
    int packSingleNalu(const uint8_t* nalu, size_t nalu_len, uint32_t timestamp, bool marker);

    /**
     * @brief 打包 FU 分片
     */
    int packFu(const uint8_t* nalu, size_t nalu_len, uint32_t timestamp, bool is_last_nalu);

    /**
     * @brief 发送一个 RTP 包
     */
    void sendPacket(const uint8_t* data, size_t len, bool marker, uint32_t timestamp);
};

}  // namespace rtp_packet

#endif  // RTP_PACKET_H265_PACKER_H_
