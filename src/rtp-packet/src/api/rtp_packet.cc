/**
 * @file rtp_packet.cc
 * @brief RTP 打包库 C API 实现
 *
 * 桥接外部 C API 与内部 C++ 实现。
 */

#include "rtp_packet.h"

#include "codec/packer_factory.h"
#include "log/module_log.h"

// ===== C API 实现 =====

extern "C"
{
int rtp_packer_create(void** packer, const RtpPackerConfig* config)
{
    if (!packer)
    {
        return RTP_INVALID_ARGUMENT;
    }

    // 默认配置
    rtp_packet::PackerConfig internal_config;

    if (config)
    {
        internal_config.codec = config->codec;
        internal_config.ssrc = config->ssrc;
        internal_config.seq_start = config->seq_start;
        internal_config.mtu = config->mtu > 0 ? config->mtu : 1400;
        internal_config.payload_type = config->payload_type;
        internal_config.callback = config->callback;
        internal_config.user_data = config->user_data;
        internal_config.auto_sps_pps = (config->auto_sps_pps != 0);
    }

    *packer = rtp_packet::createPacker(internal_config);
    if (!*packer)
    {
        LOG_ERROR("Failed to create packer");
        return RTP_OUT_OF_MEMORY;
    }

    LOG_INFO("RTP packer created, codec=%d, ssrc=0x%08x, mtu=%zu", internal_config.codec, internal_config.ssrc,
             internal_config.mtu);
    return RTP_OK;
}

int rtp_packer_destroy(void* packer)
{
    if (!packer)
    {
        return RTP_INVALID_ARGUMENT;
    }

    rtp_packet::destroyPacker(static_cast<rtp_packet::RtpPackerBase*>(packer));
    LOG_INFO("RTP packer destroyed");
    return RTP_OK;
}

int rtp_packer_pack_frame(void* packer, const uint8_t* frame, size_t len, uint32_t timestamp)
{
    if (!packer || !frame || len == 0)
    {
        return RTP_INVALID_ARGUMENT;
    }

    rtp_packet::RtpPackerBase* packer_impl = static_cast<rtp_packet::RtpPackerBase*>(packer);
    int ret = packer_impl->packFrame(frame, len, timestamp);

    return ret == 0 ? RTP_OK : RTP_ERROR;
}

int rtp_packer_set_ssrc(void* packer, uint32_t ssrc)
{
    if (!packer)
    {
        return RTP_INVALID_ARGUMENT;
    }

    rtp_packet::RtpPackerBase* packer_impl = static_cast<rtp_packet::RtpPackerBase*>(packer);
    packer_impl->setSSRC(ssrc);
    return RTP_OK;
}

int rtp_packer_set_mtu(void* packer, size_t mtu)
{
    if (!packer)
    {
        return RTP_INVALID_ARGUMENT;
    }

    rtp_packet::RtpPackerBase* packer_impl = static_cast<rtp_packet::RtpPackerBase*>(packer);
    packer_impl->setMTU(mtu);
    return RTP_OK;
}

int rtp_packer_set_auto_sps_pps(void* packer, int enable)
{
    if (!packer)
    {
        return RTP_INVALID_ARGUMENT;
    }

    rtp_packet::RtpPackerBase* packer_impl = static_cast<rtp_packet::RtpPackerBase*>(packer);
    packer_impl->setAutoSpsPps(enable != 0);
    LOG_INFO("Auto SPS/PPS %s", enable ? "enabled" : "disabled");
    return RTP_OK;
}

int rtp_packer_get_stats(void* packer, uint32_t* packet_count, uint32_t* octet_count)
{
    if (!packer || (!packet_count && !octet_count))
    {
        return RTP_INVALID_ARGUMENT;
    }

    rtp_packet::RtpPackerBase* packer_impl = static_cast<rtp_packet::RtpPackerBase*>(packer);
    rtp_packet::RtcpSrStats stats = packer_impl->getStats();

    if (packet_count)
    {
        *packet_count = stats.packet_count;
    }
    if (octet_count)
    {
        *octet_count = stats.octet_count;
    }
    return RTP_OK;
}

int rtp_packer_build_rtcp_sr(void* packer, uint8_t* buf, uint32_t rtp_timestamp)
{
    if (!packer || !buf)
    {
        return RTP_INVALID_ARGUMENT;
    }

    rtp_packet::RtpPackerBase* packer_impl = static_cast<rtp_packet::RtpPackerBase*>(packer);
    int ret = packer_impl->buildRtcpSr(buf, rtp_timestamp);

    if (ret < 0)
    {
        return RTP_ERROR;
    }
    return RTP_OK;
}

int rtp_packet_get_log_module_id(void)
{
    return rtp_packet::GetLogModuleId();
}

const char* rtp_packet_version_string(void)
{
    return RTP_PACKET_VERSION_STRING;
}

}  // extern "C"
