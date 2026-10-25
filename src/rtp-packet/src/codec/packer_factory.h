/**
 * @file packer_factory.h
 * @brief 打包器工厂方法
 */

#ifndef RTP_PACKET_PACKER_FACTORY_H_
#define RTP_PACKET_PACKER_FACTORY_H_

#include "packer_base.h"

namespace rtp_packet
{

/**
 * @brief 创建打包器
 *
 * @param config 打包参数
 * @return 打包器指针，失败返回 nullptr
 */
RtpPackerBase* createPacker(const PackerConfig& config);

/**
 * @brief 销毁打包器
 *
 * @param packer 打包器指针
 */
void destroyPacker(RtpPackerBase* packer);

}  // namespace rtp_packet

#endif  // RTP_PACKET_PACKER_FACTORY_H_
/**
 * @file packer_factory.h
 * @brief 打包器工厂方法
 */

#ifndef RTP_PACKET_PACKER_FACTORY_H_
#define RTP_PACKET_PACKER_FACTORY_H_

#include "packer_base.h"

namespace rtp_packet
{

/**
 * @brief 创建打包器
 *
 * @param config 打包参数
 * @return 打包器指针，失败返回 nullptr
 */
RtpPackerBase* createPacker(const PackerConfig& config);

/**
 * @brief 销毁打包器
 *
 * @param packer 打包器指针
 */
void destroyPacker(RtpPackerBase* packer);

}  // namespace rtp_packet

#endif  // RTP_PACKET_PACKER_FACTORY_H_
/**
 * @file packer_factory.h
 * @brief 打包器工厂方法
 */

#ifndef RTP_PACKET_PACKER_FACTORY_H_
#define RTP_PACKET_PACKER_FACTORY_H_

#include "packer_base.h"

namespace rtp_packet
{

/**
 * @brief 创建打包器
 *
 * @param config 打包参数
 * @return 打包器指针，失败返回 nullptr
 */
RtpPackerBase* createPacker(const PackerConfig& config);

/**
 * @brief 销毁打包器
 *
 * @param packer 打包器指针
 */
void destroyPacker(RtpPackerBase* packer);

}  // namespace rtp_packet

#endif  // RTP_PACKET_PACKER_FACTORY_H_
/**
 * @file packer_factory.h
 * @brief 打包器工厂方法
 */

#ifndef RTP_PACKET_PACKER_FACTORY_H_
#define RTP_PACKET_PACKER_FACTORY_H_

#include <cstddef>
#include <cstdint>

#include "rtp_packet.h"

namespace rtp_packet
{

// 内部打包器基类（后续实现中完善）
class RtpPackerBase
{
public:
    virtual ~RtpPackerBase() = default;
};

// 工厂方法
RtpPackerBase* createPacker(RtpCodecType codec, uint32_t ssrc, uint16_t seq_start, size_t mtu, uint8_t payload_type);
void destroyPacker(RtpPackerBase* packer);

}  // namespace rtp_packet

#endif  // RTP_PACKET_PACKER_FACTORY_H_
