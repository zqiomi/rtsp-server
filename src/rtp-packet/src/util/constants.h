/**
 * @file constants.h
 * @brief RTP 打包库常量定义
 *
 * 集中管理项目中所有系统级硬编码常量，避免魔法数字和字符串散落在代码各处。
 */

#ifndef RTP_PACKET_CONSTANTS_H_
#define RTP_PACKET_CONSTANTS_H_

#include <cstddef>
#include <cstdint>

namespace rtp_packet
{

// ===== RTP 协议常量 =====

/** RTP 固定头部长度（字节） */
static constexpr size_t kRtpHeaderSize = 12;

/** 默认 RTP Payload Type（动态类型） */
static constexpr uint8_t kDefaultPayloadType = 96;

/** 默认 RTP 时钟频率（Hz），H.264/H.265 均为 90kHz */
static constexpr uint32_t kDefaultClockRate = 90000;

/** 默认 MTU（字节），留有余量避免 IP 分片 */
static constexpr size_t kDefaultMTU = 1400;

/** 最大 RTP 包大小（字节） */
static constexpr size_t kMaxRtpPacketSize = 65536;

// ===== H.264 NALU 类型 =====

/** H.264 FU-A 分片类型值 */
static constexpr uint8_t kH264NalTypeFU_A = 28;

/** H.264 STAP-A 聚合类型值 */
static constexpr uint8_t kH264NalTypeSTAP_A = 24;

// ===== H.265 NALU 类型 =====

/** H.265 FU 分片类型值 */
static constexpr uint8_t kH265NalTypeFU = 49;

/** H.265 AP 聚合类型值 */
static constexpr uint8_t kH265NalTypeAP = 48;

// ===== Annex-B 分隔符 =====

/** 3 字节起始码前缀 */
static constexpr uint8_t kAnnexBStartCode3[3] = {0x00, 0x00, 0x01};

/** 4 字节起始码前缀 */
static constexpr uint8_t kAnnexBStartCode4[4] = {0x00, 0x00, 0x00, 0x01};

}  // namespace rtp_packet

#endif  // RTP_PACKET_CONSTANTS_H_
