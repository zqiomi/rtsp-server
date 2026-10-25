/**
 * @file packer_factory.cc
 * @brief 打包器工厂方法实现
 */

#include "packer_factory.h"

#include "h264_packer.h"
#include "h265_packer.h"

namespace rtp_packet
{

RtpPackerBase* createPacker(const PackerConfig& config)
{
    switch (config.codec)
    {
        case RTP_CODEC_H264:
            return new H264Packer(config);
        case RTP_CODEC_H265:
            return new H265Packer(config);
        default:
            return nullptr;
    }
}

void destroyPacker(RtpPackerBase* packer)
{
    delete packer;
}

}  // namespace rtp_packet
