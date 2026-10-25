/**
 * @file h264_packer.h
 * @brief H.264 RTP 打包器 (RFC 6184)
 *
 * 支持：
 * - Single NAL Unit Packet：NALU 长度 <= MTU 时单包发送
 * - FU-A 分片：NALU 长度 > MTU 时按 MTU 分片
 * - SPS/PPS 自动缓存和重发：IDR 帧前自动插入缓存的 SPS/PPS
 */

#ifndef RTP_PACKET_H264_PACKER_H_
#define RTP_PACKET_H264_PACKER_H_

#include <vector>

#include "packer_base.h"

namespace rtp_packet
{

/**
 * @brief H.264 RTP 打包器
 */
class H264Packer : public RtpPackerBase
{
public:
    H264Packer(const PackerConfig& config);
    ~H264Packer() override = default;

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
     *
     * @param nalu NALU 数据（不含起始码）
     * @param nalu_len NALU 长度
     * @param timestamp RTP 时间戳
     * @param is_last_nalu 是否是帧的最后一个 NALU
     * @return 0 成功，<0 失败
     */
    int packNalu(const uint8_t* nalu, size_t nalu_len, uint32_t timestamp, bool is_last_nalu);

    /**
     * @brief 打包 Single NAL Unit Packet
     */
    int packSingleNalu(const uint8_t* nalu, size_t nalu_len, uint32_t timestamp, bool marker);

    /**
     * @brief 打包 FU-A 分片
     */
    int packFuA(const uint8_t* nalu, size_t nalu_len, uint32_t timestamp, bool is_last_fragment);

    /**
     * @brief 发送一个 RTP 包
     */
    void sendPacket(const uint8_t* data, size_t len, bool marker, uint32_t timestamp);

    /**
     * @brief 缓存 SPS/PPS NALU
     */
    void cacheSpsPps(const uint8_t* nalu, size_t nalu_len, uint8_t nalu_type);

    /**
     * @brief 发送缓存的 SPS/PPS
     */
    int sendCachedSpsPps(uint32_t timestamp);

    // SPS/PPS 缓存
    std::vector<uint8_t> cached_sps_;  // SPS NALU（不含起始码）
    std::vector<uint8_t> cached_pps_;  // PPS NALU（不含起始码）
};

}  // namespace rtp_packet

#endif  // RTP_PACKET_H264_PACKER_H_
/**
 * @file h264_packer.h
 * @brief H.264 RTP 打包器 (RFC 6184)
 *
 * 支持：
 * - Single NAL Unit Packet：NALU 长度 <= MTU 时单包发送
 * - FU-A 分片：NALU 长度 > MTU 时按 MTU 分片
 */

#ifndef RTP_PACKET_H264_PACKER_H_
#define RTP_PACKET_H264_PACKER_H_

#include "packer_base.h"

namespace rtp_packet
{

/**
 * @brief H.264 RTP 打包器
 */
class H264Packer : public RtpPackerBase
{
public:
    H264Packer(const PackerConfig& config);
    ~H264Packer() override = default;

    int packFrame(const uint8_t* frame, size_t len, uint32_t timestamp) override;

    void setSSRC(uint32_t ssrc) override;
    void setMTU(size_t mtu) override;
    uint32_t getSSRC() const override;
    size_t getMTU() const override;

private:
    /**
     * @brief 打包单个 NALU
     *
     * @param nalu NALU 数据（不含起始码）
     * @param nalu_len NALU 长度
     * @param timestamp RTP 时间戳
     * @param is_last_nalu 是否是帧的最后一个 NALU
     * @param callback 回调
     * @param user_data 用户数据
     * @return 0 成功，<0 失败
     */
    int packNalu(const uint8_t* nalu, size_t nalu_len, uint32_t timestamp, bool is_last_nalu);

    /**
     * @brief 打包 Single NAL Unit Packet
     */
    int packSingleNalu(const uint8_t* nalu, size_t nalu_len, uint32_t timestamp, bool marker);

    /**
     * @brief 打包 FU-A 分片
     */
    int packFuA(const uint8_t* nalu, size_t nalu_len, uint32_t timestamp, bool is_last_fragment);

    /**
     * @brief 发送一个 RTP 包
     */
    void sendPacket(const uint8_t* data, size_t len, bool marker, uint32_t timestamp);
};

}  // namespace rtp_packet

#endif  // RTP_PACKET_H264_PACKER_H_
