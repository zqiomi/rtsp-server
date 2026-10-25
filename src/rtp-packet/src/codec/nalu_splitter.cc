#include "nalu_splitter.h"

namespace rtp_packet
{

/**
 * @brief 从 pos 开始查找下一个起始码的位置
 *
 * @param data 数据起始
 * @param len 数据长度
 * @param pos 搜索起始位置
 * @return 下一个起始码的位置，如果未找到返回 len
 */
static size_t findNextStartCodePos(const uint8_t* data, size_t len, size_t pos)
{
    for (size_t i = pos; i + 2 < len; i++)
    {
        if (data[i] == 0x00 && data[i + 1] == 0x00)
        {
            if (data[i + 2] == 0x01)
            {
                return i;
            }
            // 检查 4 字节起始码
            if (i + 3 < len && data[i + 2] == 0x00 && data[i + 3] == 0x01)
            {
                return i;
            }
        }
    }
    return len;
}

bool findNextNalu(const uint8_t* data, size_t len, size_t& start_pos, NaluInfo& info)
{
    if (!data || len == 0)
    {
        return false;
    }

    // 首次调用：跳过开头的起始码
    if (start_pos == 0)
    {
        // 检查开头是否有起始码
        if (len >= 4 && data[0] == 0x00 && data[1] == 0x00 && data[2] == 0x00 && data[3] == 0x01)
        {
            start_pos = 4;
        }
        else if (len >= 3 && data[0] == 0x00 && data[1] == 0x00 && data[2] == 0x01)
        {
            start_pos = 3;
        }
        else
        {
            // 没有起始码前缀，从头开始
            start_pos = 0;
        }
    }

    // 当前 NALU 数据起始
    size_t nalu_start = start_pos;
    if (nalu_start >= len)
    {
        return false;
    }

    // 查找下一个起始码
    size_t next_start_code = findNextStartCodePos(data, len, nalu_start);

    // NALU 数据结束（不含起始码）
    size_t nalu_end = next_start_code;

    // 设置输出
    info.data = data + nalu_start;
    info.size = nalu_end - nalu_start;

    // 更新 start_pos 为下一个 NALU 的数据起始
    if (next_start_code < len)
    {
        // 跳过下一个起始码
        if (next_start_code + 4 <= len && data[next_start_code] == 0x00 && data[next_start_code + 1] == 0x00 &&
            data[next_start_code + 2] == 0x00 && data[next_start_code + 3] == 0x01)
        {
            start_pos = next_start_code + 4;
        }
        else if (next_start_code + 3 <= len && data[next_start_code] == 0x00 && data[next_start_code + 1] == 0x00 &&
                 data[next_start_code + 2] == 0x01)
        {
            start_pos = next_start_code + 3;
        }
        else
        {
            start_pos = next_start_code;
        }
    }
    else
    {
        start_pos = len;
    }

    return info.size > 0;
}

}  // namespace rtp_packet
