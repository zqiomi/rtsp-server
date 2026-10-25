/**
 * @file nalu_splitter.h
 * @brief Annex-B 格式 NALU 分割工具
 *
 * 支持两种起始码格式：
 * - 3 字节：0x00 0x00 0x01
 * - 4 字节：0x00 0x00 0x00 0x01
 *
 * 分割后的 NALU 不包含起始码，直接是 NALU Header + RBSP。
 */

#ifndef RTP_PACKET_NALU_SPLITTER_H_
#define RTP_PACKET_NALU_SPLITTER_H_

#include <cstddef>
#include <cstdint>

namespace rtp_packet
{

/**
 * @brief NALU 信息结构体
 */
struct NaluInfo
{
    const uint8_t* data; /**< NALU 数据起始地址（不含起始码） */
    size_t size;         /**< NALU 数据长度（不含起始码） */
};

/**
 * @brief 从 Annex-B 格式的帧数据中查找下一个 NALU
 *
 * 从 start_pos 开始向后搜索起始码，找到后返回 NALU 的信息。
 * 首次调用时 start_pos 应为 0，函数会跳过开头的起始码。
 *
 * @param[in] data 帧数据
 * @param[in] len 帧数据长度
 * @param[in,out] start_pos 搜索起始位置，返回时更新为下一个 NALU 的起始位置
 * @param[out] info 输出 NALU 信息
 * @return true 找到 NALU，false 已到达末尾
 *
 * @code
 * size_t pos = 0;
 * NaluInfo info;
 * while (findNextNalu(data, len, pos, info)) {
 *     // 处理 info.data, info.size
 * }
 * @endcode
 */
bool findNextNalu(const uint8_t* data, size_t len, size_t& start_pos, NaluInfo& info);

/**
 * @brief 获取 NALU 类型（H.264）
 *
 * H.264 NALU Header 为 1 字节：forbidden_zero_bit(1) | nal_ref_idc(2) | nal_unit_type(5)
 *
 * @param nalu_header NALU 第一个字节
 * @return NALU 类型（0-31）
 */
inline uint8_t getH264NaluType(uint8_t nalu_header)
{
    return nalu_header & 0x1F;
}

/**
 * @brief 获取 NALU 类型（H.265）
 *
 * H.265 NALU Header 为 2 字节：forbidden_zero_bit(1) | nal_unit_type(6) | nuh_layer_id(6) | nuh_temporal_id_plus1(3)
 *
 * @param nalu_header NALU 前两个字节
 * @return NALU 类型（0-63）
 */
inline uint8_t getH265NaluType(const uint8_t* nalu_header)
{
    return (nalu_header[0] >> 1) & 0x3F;
}

}  // namespace rtp_packet

#endif  // RTP_PACKET_NALU_SPLITTER_H_
