/**
 * @file rtp_packet.h
 * @brief RTP 打包库 对外接口头文件
 *
 * 轻量级 RTP 打包库，支持 H.264/H.265 编码帧的 RTP 封包。
 *
 * 使用方式：
 * 1. 调用 rtp_packer_create() 创建打包器实例
 * 2. 调用 rtp_packer_pack_frame() 打包一帧数据（Annex-B 格式）
 * 3. 每产出一个 RTP 包，回调函数会被调用
 * 4. 调用 rtp_packer_destroy() 销毁打包器
 *
 * 所有API返回int错误码：
 * - RTP_OK (0): 成功
 * - 负数: 失败，具体错误码见 RtpErrorCode 枚举
 */

#ifndef RTP_PACKET_API_H_
#define RTP_PACKET_API_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stddef.h>
#include <stdint.h>

/**
 * @brief RTP 打包库 错误码定义
 */
typedef enum RtpErrorCode
{
    RTP_OK = 0,                /**< 操作成功 */
    RTP_ERROR = -1,            /**< 通用错误 */
    RTP_INVALID_ARGUMENT = -2, /**< 参数无效（如NULL指针、非法值等） */
    RTP_BUFFER_TOO_SMALL = -3, /**< 缓冲区太小 */
    RTP_PARSE_ERROR = -4,      /**< 数据解析错误 */
    RTP_NOT_IMPLEMENTED = -5,  /**< 功能未实现 */
    RTP_OUT_OF_MEMORY = -6,    /**< 内存分配失败 */
} RtpErrorCode;

/**
 * @brief 编解码类型
 */
typedef enum RtpCodecType
{
    RTP_CODEC_H264 = 0, /**< H.264 / AVC */
    RTP_CODEC_H265 = 1, /**< H.265 / HEVC */
} RtpCodecType;

/**
 * @brief RTP 打包回调函数类型
 *
 * 每产出一个完整的 RTP 包时调用。
 *
 * @param[in] data RTP 包数据指针（包含 RTP 头）
 * @param[in] len RTP 包长度（字节）
 * @param[in] user_data 用户自定义数据指针，创建时通过 RtpPackerConfig 传入
 */
typedef void (*RtpPacketCallback)(const uint8_t* data, size_t len, void* user_data);

/**
 * @brief RTP 打包器配置结构体
 *
 * 用于创建打包器时传递配置参数。所有字段均为可选，未设置的字段将使用默认值。
 */
typedef struct RtpPackerConfig
{
    RtpCodecType codec;         /**< 编解码类型，默认 RTP_CODEC_H264 */
    uint32_t ssrc;              /**< SSRC，默认 0 */
    uint16_t seq_start;         /**< 起始序列号，默认 0（随机） */
    size_t mtu;                 /**< 最大传输单元（字节），默认 1400 */
    uint8_t payload_type;       /**< RTP Payload Type，默认 96 */
    RtpPacketCallback callback; /**< 数据回调接口 */
    void* user_data;            /* 用户数据 */
    int auto_sps_pps;           /**< 自动在 IDR 帧前插入 SPS/PPS，默认 1（开启） */
} RtpPackerConfig;

/**
 * @brief 创建 RTP 打包器实例
 *
 * @param[out] packer 输出参数，用于返回创建的打包器句柄
 * @param[in] config 配置结构体指针，可为NULL（使用默认配置，H.264）
 * @return RTP_OK 表示成功，其他值表示失败
 *
 * @code
 * // 使用默认配置创建 H.264 打包器
 * void* packer = NULL;
 * int ret = rtp_packer_create(&packer, NULL);
 *
 * // 使用自定义配置创建 H.265 打包器
 * RtpPackerConfig config = {
 *     .codec = RTP_CODEC_H265,
 *     .ssrc = 0x12345678,
 *     .seq_start = 0,
 *     .mtu = 1400,
 *     .payload_type = 96,
 * };
 * int ret = rtp_packer_create(&packer, &config);
 * @endcode
 */
int rtp_packer_create(void** packer, const RtpPackerConfig* config);

/**
 * @brief 销毁 RTP 打包器实例
 *
 * @param[in] packer 打包器句柄，由 rtp_packer_create 创建
 * @return RTP_OK 表示成功，其他值表示失败
 *
 * @note 调用此函数后，packer 句柄不再有效
 */
int rtp_packer_destroy(void* packer);

/**
 * @brief 打包一帧数据
 *
 * 将一帧 Annex-B 格式的编码数据打包为一个或多个 RTP 包。
 * 每产出一个 RTP 包，回调函数会被调用一次。
 *
 * @param[in] packer 打包器句柄
 * @param[in] frame Annex-B 格式的帧数据（含起始码）
 * @param[in] len 帧数据长度（字节）
 * @param[in] timestamp RTP 时间戳（90kHz 时钟）
 * @return RTP_OK 表示成功，其他值表示失败
 *
 * @code
 * // 打包一帧 H.264 数据
 * uint8_t frame[] = {0x00, 0x00, 0x00, 0x01, 0x65, ...};
 * size_t frame_len = sizeof(frame);
 *
 * int ret = rtp_packer_pack_frame(packer, frame, frame_len, timestamp);
 * @endcode
 */
int rtp_packer_pack_frame(void* packer, const uint8_t* frame, size_t len, uint32_t timestamp);

/**
 * @brief 设置 SSRC
 *
 * @param[in] packer 打包器句柄
 * @param[in] ssrc SSRC 值
 * @return RTP_OK 表示成功，其他值表示失败
 */
int rtp_packer_set_ssrc(void* packer, uint32_t ssrc);

/**
 * @brief 设置 MTU
 *
 * @param[in] packer 打包器句柄
 * @param[in] mtu 最大传输单元（字节）
 * @return RTP_OK 表示成功，其他值表示失败
 */
int rtp_packer_set_mtu(void* packer, size_t mtu);

/**
 * @brief 设置是否自动在 IDR 帧前插入缓存的 SPS/PPS
 *
 * @param[in] packer 打包器句柄
 * @param[in] enable 1=开启，0=关闭
 * @return RTP_OK 表示成功，其他值表示失败
 *
 * @note 开启后，打包器会自动缓存首次收到的 SPS/PPS，
 *       并在后续 IDR 帧前自动插入（如果帧数据中不包含 SPS/PPS）
 */
int rtp_packer_set_auto_sps_pps(void* packer, int enable);

/**
 * @brief 获取发送统计信息
 *
 * @param[in] packer 打包器句柄
 * @param[out] packet_count 已发送的 RTP 包数
 * @param[out] octet_count 已发送的 RTP 字节数（payload only）
 * @return RTP_OK 表示成功，其他值表示失败
 */
int rtp_packer_get_stats(void* packer, uint32_t* packet_count, uint32_t* octet_count);

/**
 * @brief 构造 RTCP Sender Report
 *
 * @param[in] packer 打包器句柄
 * @param[out] buf 输出缓冲区，至少 28 字节
 * @param[in] rtp_timestamp 当前 RTP 时间戳
 * @return RTCP SR 包长度（28），负数表示失败
 *
 * @note RTCP SR 包含发送统计：包数、字节数、NTP 时间戳等
 */
int rtp_packer_build_rtcp_sr(void* packer, uint8_t* buf, uint32_t rtp_timestamp);

/**
 * @brief RTP打包库 获取日志模块 ID
 *
 * 获取 rtp_packet 模块在 rslog 中注册的模块 ID。
 * 调用方拿到 ID 后，可通过 rslog 接口控制该模块的日志级别、输出等。
 *
 * @return 模块 ID（>=1），首次调用时自动注册
 *
 * @code
 * int log_id = rtp_packet_get_log_module_id();
 * rslog::SetModuleLevel(log_id, rslog::kDebug);
 * rslog::SetLogFile("/var/log/rtp.log");
 * @endcode
 */
int rtp_packet_get_log_module_id(void);

/**
 * @brief 获取版本号字符串
 *
 * @return 版本号字符串，如 "1.0.0"，静态存储，无需释放
 */
const char* rtp_packet_version_string(void);

#ifdef __cplusplus
}
#endif

#endif  // RTP_PACKET_API_H_
