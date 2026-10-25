/**
 * @file main.cc
 * @brief RTSP 推流服务器（H.264 over RTP/RTSP）
 *
 * 数据流：
 *   H.264 文件 → 按访问单元(AU)切帧 → rtp_packet 打 RTP 包
 *              → rtsp_forward 转发给所有 PLAYING 会话（多客户端）
 *
 * 架构：
 *   主线程   ：rtsp_forward_run() 事件循环，处理 RTSP 信令（epoll）
 *   推流线程 ：按目标帧率读取文件、打包、发送 RTP
 *
 * 用法：
 *   rtsp_server -f test.h264 -p 8554
 *   ffplay rtsp://127.0.0.1:8554/stream
 */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

#include "log/log.h"
#include "rtp_packet.h"
#include "rtsp_forward.h"

namespace
{

/** RTP 视频时钟频率（H.264 固定 90kHz） */
const uint32_t kRtpClockRate = 90000;

/** 运行时状态 */
std::atomic<bool> g_running(true);
void* g_server = NULL;
int g_app_log_id = 0;

/**
 * @brief 命令行选项
 */
struct Options
{
    std::string file;                        /**< H.264 文件路径 */
    std::string rtsp_path = "stream";        /**< RTSP 播放路径（仅用于拼接 URL） */
    std::string log_file;                    /**< 日志文件，空表示输出到终端 */
    int port = 8554;                         /**< 监听端口 */
    int fps = 25;                            /**< 推流帧率 */
    int max_sessions = 10;                   /**< 最大并发会话 */
    int mtu = 1400;                          /**< RTP MTU */
    uint32_t ssrc = 0x12345678;              /**< RTP SSRC */
    rslog::Level log_level = rslog::kInfo;   /**< 日志级别 */
    bool loop = true;                        /**< 文件尾是否循环播放 */

    Options()
    {
    }
};

/**
 * @brief 一个访问单元（一帧）在文件中的位置
 */
struct FrameSpan
{
    size_t offset; /**< 起始偏移（含起始码） */
    size_t length; /**< 长度（字节） */
    bool keyframe; /**< 是否包含 IDR */
};

/**
 * @brief 序列参数集（SPS/PPS）
 *
 * 用于填充 SDP 的 sprop-parameter-sets，客户端无需等待带内参数集即可初始化解码器。
 */
struct ParamSets
{
    std::vector<uint8_t> sps;
    std::vector<uint8_t> pps;

    bool Valid() const
    {
        return sps.size() >= 4 && !pps.empty();
    }
};

/**
 * @brief Base64 编码（仅用于 SDP 的 sprop-parameter-sets）
 */
std::string Base64Encode(const uint8_t* data, size_t len)
{
    static const char kTable[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string out;
    out.reserve(((len + 2) / 3) * 4);

    size_t i = 0;
    while (i + 3 <= len)
    {
        uint32_t v = (static_cast<uint32_t>(data[i]) << 16) |
                     (static_cast<uint32_t>(data[i + 1]) << 8) |
                     static_cast<uint32_t>(data[i + 2]);
        out += kTable[(v >> 18) & 0x3F];
        out += kTable[(v >> 12) & 0x3F];
        out += kTable[(v >> 6) & 0x3F];
        out += kTable[v & 0x3F];
        i += 3;
    }

    const size_t rest = len - i;
    if (rest == 1)
    {
        uint32_t v = static_cast<uint32_t>(data[i]) << 16;
        out += kTable[(v >> 18) & 0x3F];
        out += kTable[(v >> 12) & 0x3F];
        out += "==";
    }
    else if (rest == 2)
    {
        uint32_t v = (static_cast<uint32_t>(data[i]) << 16) |
                     (static_cast<uint32_t>(data[i + 1]) << 8);
        out += kTable[(v >> 18) & 0x3F];
        out += kTable[(v >> 12) & 0x3F];
        out += kTable[(v >> 6) & 0x3F];
        out += '=';
    }

    return out;
}

void PrintUsage(const char* prog)
{
    printf("RTSP 推流服务器（H.264）\n\n");
    printf("用法: %s -f <file.h264> [选项]\n\n", prog);
    printf("必选:\n");
    printf("  -f, --file <path>         H.264 Annex-B 码流文件\n\n");
    printf("可选:\n");
    printf("  -p, --port <port>         监听端口（默认 8554）\n");
    printf("      --path <name>         RTSP 路径名（默认 stream）\n");
    printf("      --fps <n>             推流帧率（默认 25）\n");
    printf("      --max-sessions <n>    最大并发会话（默认 10）\n");
    printf("      --mtu <n>             RTP MTU（默认 1400）\n");
    printf("      --ssrc <hex>          RTP SSRC（默认 0x12345678）\n");
    printf("      --no-loop             播完不循环\n");
    printf("      --log-level <level>   trace|debug|info|warn|error（默认 info）\n");
    printf("      --log-file <path>     日志写入文件\n");
    printf("  -h, --help                显示帮助\n\n");
    printf("示例:\n");
    printf("  %s -f test.h264 -p 8554 --fps 25\n", prog);
    printf("  播放: ffplay rtsp://127.0.0.1:8554/stream\n");
}

/**
 * @brief 解析命令行
 * @return 0 正常，1 参数错误，2 已显示帮助
 */
int ParseArgs(int argc, char** argv, Options* opt)
{
    for (int i = 1; i < argc; ++i)
    {
        const char* arg = argv[i];
        // 需要取下一个参数的选项
        bool has_next = (i + 1 < argc);

        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0)
        {
            PrintUsage(argv[0]);
            return 2;
        }
        else if (strcmp(arg, "-f") == 0 || strcmp(arg, "--file") == 0)
        {
            if (!has_next)
            {
                fprintf(stderr, "错误: %s 需要参数\n", arg);
                return 1;
            }
            opt->file = argv[++i];
        }
        else if (strcmp(arg, "-p") == 0 || strcmp(arg, "--port") == 0)
        {
            if (!has_next)
            {
                fprintf(stderr, "错误: %s 需要参数\n", arg);
                return 1;
            }
            opt->port = atoi(argv[++i]);
        }
        else if (strcmp(arg, "--path") == 0)
        {
            if (!has_next)
            {
                fprintf(stderr, "错误: %s 需要参数\n", arg);
                return 1;
            }
            opt->rtsp_path = argv[++i];
        }
        else if (strcmp(arg, "--fps") == 0)
        {
            if (!has_next)
            {
                fprintf(stderr, "错误: %s 需要参数\n", arg);
                return 1;
            }
            opt->fps = atoi(argv[++i]);
        }
        else if (strcmp(arg, "--max-sessions") == 0)
        {
            if (!has_next)
            {
                fprintf(stderr, "错误: %s 需要参数\n", arg);
                return 1;
            }
            opt->max_sessions = atoi(argv[++i]);
        }
        else if (strcmp(arg, "--mtu") == 0)
        {
            if (!has_next)
            {
                fprintf(stderr, "错误: %s 需要参数\n", arg);
                return 1;
            }
            opt->mtu = atoi(argv[++i]);
        }
        else if (strcmp(arg, "--ssrc") == 0)
        {
            if (!has_next)
            {
                fprintf(stderr, "错误: %s 需要参数\n", arg);
                return 1;
            }
            opt->ssrc = static_cast<uint32_t>(strtoul(argv[++i], NULL, 0));
        }
        else if (strcmp(arg, "--no-loop") == 0)
        {
            opt->loop = false;
        }
        else if (strcmp(arg, "--log-level") == 0)
        {
            if (!has_next || !rslog::ParseLevel(argv[++i], &opt->log_level))
            {
                fprintf(stderr, "错误: 无效的日志级别\n");
                return 1;
            }
        }
        else if (strcmp(arg, "--log-file") == 0)
        {
            if (!has_next)
            {
                fprintf(stderr, "错误: %s 需要参数\n", arg);
                return 1;
            }
            opt->log_file = argv[++i];
        }
        else
        {
            fprintf(stderr, "错误: 未知参数 %s（用 -h 查看帮助）\n", arg);
            return 1;
        }
    }

    if (opt->file.empty())
    {
        fprintf(stderr, "错误: 必须通过 -f 指定 H.264 文件\n");
        return 1;
    }
    if (opt->port <= 0 || opt->port > 65535)
    {
        fprintf(stderr, "错误: 端口范围 1-65535\n");
        return 1;
    }
    if (opt->fps <= 0 || opt->fps > 120)
    {
        fprintf(stderr, "错误: 帧率范围 1-120\n");
        return 1;
    }
    if (opt->max_sessions <= 0)
    {
        fprintf(stderr, "错误: 最大会话数必须大于 0\n");
        return 1;
    }

    return 0;
}

/**
 * @brief 读取整个文件
 */
int LoadFile(const std::string& path, std::vector<uint8_t>* out)
{
    FILE* fp = fopen(path.c_str(), "rb");
    if (fp == NULL)
    {
        RSLOG_ERROR(g_app_log_id, "无法打开文件: %s", path.c_str());
        return -1;
    }

    if (fseek(fp, 0, SEEK_END) != 0)
    {
        fclose(fp);
        return -1;
    }
    long size = ftell(fp);
    if (size <= 0)
    {
        RSLOG_ERROR(g_app_log_id, "文件为空: %s", path.c_str());
        fclose(fp);
        return -1;
    }
    rewind(fp);

    out->resize(static_cast<size_t>(size));
    size_t got = fread(out->data(), 1, static_cast<size_t>(size), fp);
    fclose(fp);

    if (got != static_cast<size_t>(size))
    {
        RSLOG_ERROR(g_app_log_id, "文件读取不完整(%zu/%ld)", got, size);
        return -1;
    }

    RSLOG_INFO(g_app_log_id, "已加载码流: %s (%zu 字节)", path.c_str(), out->size());
    return 0;
}

/**
 * @brief NALU 在文件中的范围
 */
struct NaluSpan
{
    size_t begin;    /**< 起始码起点 */
    size_t payload;  /**< NALU 头位置 */
    size_t end;      /**< 结束位置（不含） */
    uint8_t type;    /**< NALU 类型 */
};

/**
 * @brief 按 Annex-B 起始码切分 NALU
 *
 * 0x000003 是防竞争字节，编码器保证载荷内不会出现 0x000001，
 * 因此直接扫描起始码是安全的。
 */
void SplitNalus(const std::vector<uint8_t>& data, std::vector<NaluSpan>* nalus)
{
    const size_t n = data.size();
    size_t sc_begin = 0;
    size_t sc_payload = 0;
    bool found = false;

    size_t i = 0;
    while (i + 2 < n)
    {
        if (data[i] == 0x00 && data[i + 1] == 0x00 && data[i + 2] == 0x01)
        {
            // 4 字节起始码时把前导 0x00 一并算入
            size_t begin = (i > 0 && data[i - 1] == 0x00) ? (i - 1) : i;

            if (found)
            {
                NaluSpan span;
                span.begin = sc_begin;
                span.payload = sc_payload;
                span.end = begin;
                span.type = static_cast<uint8_t>(data[sc_payload] & 0x1F);
                if (span.end > span.payload)
                {
                    nalus->push_back(span);
                }
            }

            found = true;
            sc_begin = begin;
            sc_payload = i + 3;
            i = sc_payload;
            continue;
        }
        ++i;
    }

    if (found && sc_payload < n)
    {
        NaluSpan span;
        span.begin = sc_begin;
        span.payload = sc_payload;
        span.end = n;
        span.type = static_cast<uint8_t>(data[sc_payload] & 0x1F);
        if (span.end > span.payload)
        {
            nalus->push_back(span);
        }
    }
}

/**
 * @brief 判断 NALU 类型是否为 VCL（图像数据）
 */
bool IsVcl(uint8_t type)
{
    return type >= 1 && type <= 5;
}

/**
 * @brief 判断 NALU 类型是否属于"下一帧的前缀"
 *
 * SPS/PPS/SEI/AUD 若出现在已有图像数据的帧之后，应归属下一帧。
 */
bool IsAuPrefix(uint8_t type)
{
    return type == 6 || type == 7 || type == 8 || type == 9;
}

/**
 * @brief 把 NALU 合并为访问单元（一帧）
 *
 * 同一帧的所有 NALU 共享同一个 RTP 时间戳，这是 RTP 时间戳语义的要求。
 */
int BuildAccessUnits(const std::vector<uint8_t>& data, std::vector<FrameSpan>* frames,
                     ParamSets* params)
{
    std::vector<NaluSpan> nalus;
    SplitNalus(data, &nalus);

    if (nalus.empty())
    {
        RSLOG_ERROR(g_app_log_id, "未找到有效的 NALU 起始码，文件可能不是 Annex-B 格式");
        return -1;
    }

    size_t au_begin = 0;
    size_t au_end = 0;
    bool au_open = false;
    int vcl_in_au = 0;
    bool have_idr = false;

    for (size_t i = 0; i < nalus.size(); ++i)
    {
        const NaluSpan& nalu = nalus[i];

        // 记录首组 SPS/PPS，用于 SDP 的 sprop-parameter-sets
        if (nalu.type == 7 && params->sps.empty())
        {
            params->sps.assign(data.begin() + static_cast<long>(nalu.payload),
                               data.begin() + static_cast<long>(nalu.end));
        }
        else if (nalu.type == 8 && params->pps.empty())
        {
            params->pps.assign(data.begin() + static_cast<long>(nalu.payload),
                               data.begin() + static_cast<long>(nalu.end));
        }

        if (au_open)
        {
            bool start_new = (IsVcl(nalu.type) && vcl_in_au > 0) ||
                             (IsAuPrefix(nalu.type) && vcl_in_au > 0);
            if (start_new)
            {
                FrameSpan span;
                span.offset = au_begin;
                span.length = au_end - au_begin;
                span.keyframe = have_idr;
                frames->push_back(span);

                au_open = false;
                vcl_in_au = 0;
                have_idr = false;
            }
        }

        if (!au_open)
        {
            au_begin = nalu.begin;
            au_open = true;
        }
        au_end = nalu.end;

        if (IsVcl(nalu.type))
        {
            ++vcl_in_au;
        }
        if (nalu.type == 5)
        {
            have_idr = true;
        }
    }

    if (au_open && au_end > au_begin)
    {
        FrameSpan span;
        span.offset = au_begin;
        span.length = au_end - au_begin;
        span.keyframe = have_idr;
        frames->push_back(span);
    }

    int keyframes = 0;
    for (size_t i = 0; i < frames->size(); ++i)
    {
        if ((*frames)[i].keyframe)
        {
            ++keyframes;
        }
    }

    RSLOG_INFO(g_app_log_id, "解析完成: %zu 个 NALU → %zu 帧（关键帧 %d 个）",
               nalus.size(), frames->size(), keyframes);

    if (params->Valid())
    {
        RSLOG_DEBUG(g_app_log_id, "参数集: SPS %zu 字节, PPS %zu 字节（将写入 SDP）",
                    params->sps.size(), params->pps.size());
    }
    else
    {
        RSLOG_WARN(g_app_log_id, "码流中未找到 SPS/PPS，SDP 不含 sprop-parameter-sets");
    }

    return 0;
}

/**
 * @brief 单调时钟（微秒）
 */
uint64_t NowUs()
{
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

/**
 * @brief 推流器：按帧率把文件内容打包成 RTP 并交给 rtsp_forward 分发
 */
class Streamer
{
public:
    Streamer(const Options& opt, const std::vector<uint8_t>& data,
             const std::vector<FrameSpan>& frames)
        : opt_(opt), data_(data), frames_(frames), packer_(NULL)
    {
    }

    /**
     * @brief 启动推流线程
     */
    bool Start()
    {
        RtpPackerConfig config = {};
        config.codec = RTP_CODEC_H264;
        config.ssrc = opt_.ssrc;
        config.seq_start = 0;
        config.mtu = static_cast<size_t>(opt_.mtu);
        config.payload_type = 96;
        config.callback = &Streamer::OnRtpPacket;
        config.user_data = NULL;
        config.auto_sps_pps = 1;

        int ret = rtp_packer_create(&packer_, &config);
        if (ret != RTP_OK)
        {
            RSLOG_ERROR(g_app_log_id, "创建 RTP 打包器失败: %d", ret);
            return false;
        }

        thread_ = std::thread(&Streamer::Run, this);
        return true;
    }

    /**
     * @brief 通知退出并等待线程结束
     */
    void Stop()
    {
        if (thread_.joinable())
        {
            thread_.join();
        }
        if (packer_ != NULL)
        {
            rtp_packer_destroy(packer_);
            packer_ = NULL;
        }
    }

private:
    /**
     * @brief RTP 包回调：交给 rtsp_forward 转发给所有播放会话
     */
    static void OnRtpPacket(const uint8_t* data, size_t len, void* user_data)
    {
        (void)user_data;
        if (g_server != NULL && len > 0)
        {
            rtsp_forward_send_rtp(g_server, data, len, 0);
        }
    }

    /**
     * @brief 推流主循环
     */
    void Run()
    {
        const uint64_t frame_interval_us = 1000000ULL / static_cast<uint64_t>(opt_.fps);
        const uint32_t ts_increment = kRtpClockRate / static_cast<uint32_t>(opt_.fps);

        uint32_t timestamp = 0;
        uint64_t deadline_us = NowUs();
        uint64_t frames_sent = 0;
        uint64_t last_stat_us = deadline_us;
        int loop_count = 0;

        RSLOG_INFO(g_app_log_id, "推流线程启动: %d fps, 每帧时间戳增量 %u",
                   opt_.fps, ts_increment);

        while (g_running.load())
        {
            ++loop_count;
            RSLOG_DEBUG(g_app_log_id, "开始第 %d 轮推流", loop_count);

            for (size_t i = 0; i < frames_.size(); ++i)
            {
                if (!g_running.load())
                {
                    break;
                }

                const FrameSpan& frame = frames_[i];
                int ret = rtp_packer_pack_frame(packer_, data_.data() + frame.offset,
                                                frame.length, timestamp);
                if (ret != RTP_OK)
                {
                    RSLOG_WARN(g_app_log_id, "打包第 %zu 帧失败: %d", i, ret);
                }

                timestamp += ts_increment;
                ++frames_sent;

                // 绝对时间调度，避免逐帧累加误差
                deadline_us += frame_interval_us;
                uint64_t now = NowUs();
                if (now < deadline_us)
                {
                    std::this_thread::sleep_for(std::chrono::microseconds(deadline_us - now));
                }
                else if (now - deadline_us > frame_interval_us)
                {
                    deadline_us = now;  // 落后超过一帧则重新对齐
                }

                // 每 5 秒输出一次运行统计
                now = NowUs();
                if (now - last_stat_us >= 5000000ULL)
                {
                    last_stat_us = now;
                    LogStats(frames_sent);
                }
            }

            if (!opt_.loop)
            {
                RSLOG_INFO(g_app_log_id, "文件播放完毕（未启用循环），停止推流");
                g_running.store(false);
                if (g_server != NULL)
                {
                    rtsp_forward_stop(g_server);  // 唤醒事件循环，让进程退出
                }
                break;
            }
        }

        LogStats(frames_sent);
        RSLOG_INFO(g_app_log_id, "推流线程退出，累计发送 %llu 帧",
                   static_cast<unsigned long long>(frames_sent));
    }

    /**
     * @brief 输出会话与发包统计
     */
    void LogStats(uint64_t frames_sent)
    {
        RtspForwardInfo info;
        memset(&info, 0, sizeof(info));
        if (rtsp_forward_get_info(g_server, &info) != RTSP_OK)
        {
            return;
        }

        uint32_t pkt = 0;
        uint32_t octet = 0;
        if (packer_ != NULL)
        {
            rtp_packer_get_stats(packer_, &pkt, &octet);
        }

        RSLOG_INFO(g_app_log_id,
                   "统计: 已推 %llu 帧 | 会话 活跃=%d 播放中=%d 累计连接=%llu | RTP 包=%u 字节=%u",
                   static_cast<unsigned long long>(frames_sent), info.active_sessions,
                   info.playing_sessions,
                   static_cast<unsigned long long>(info.total_connections), pkt, octet);
    }

    const Options& opt_;
    const std::vector<uint8_t>& data_;
    const std::vector<FrameSpan>& frames_;
    void* packer_;
    std::thread thread_;
};

/**
 * @brief 信号处理：置退出标志并唤醒事件循环
 */
void OnSignal(int sig)
{
    (void)sig;
    g_running.store(false);
    if (g_server != NULL)
    {
        rtsp_forward_stop(g_server);
    }
}

/**
 * @brief 构造 SDP
 */
std::string BuildSdp(const ParamSets& params)
{
    std::string sdp;
    sdp += "v=0\r\n";
    sdp += "o=- 0 0 IN IP4 0.0.0.0\r\n";
    sdp += "s=RTSP Server\r\n";
    sdp += "c=IN IP4 0.0.0.0\r\n";
    sdp += "t=0 0\r\n";
    sdp += "a=control:*\r\n";
    sdp += "m=video 0 RTP/AVP 96\r\n";
    sdp += "a=rtpmap:96 H264/90000\r\n";
    sdp += "a=fmtp:96 packetization-mode=1";

    // 带上参数集，客户端不必等带内 SPS/PPS 就能初始化解码器
    if (params.Valid())
    {
        char profile_level_id[16];
        snprintf(profile_level_id, sizeof(profile_level_id), "%02X%02X%02X",
                 params.sps[1], params.sps[2], params.sps[3]);

        sdp += ";profile-level-id=";
        sdp += profile_level_id;
        sdp += ";sprop-parameter-sets=";
        sdp += Base64Encode(params.sps.data(), params.sps.size());
        sdp += ",";
        sdp += Base64Encode(params.pps.data(), params.pps.size());
    }

    sdp += "\r\n";
    sdp += "a=control:streamid=0\r\n";
    return sdp;
}

}  // namespace

int main(int argc, char* argv[])
{
    g_app_log_id = rslog::RegisterModule("rtsp_server");

    Options opt;
    int parse_ret = ParseArgs(argc, argv, &opt);
    if (parse_ret != 0)
    {
        return (parse_ret == 2) ? 0 : 1;
    }

    rslog::SetGlobalLevel(opt.log_level);
    if (!opt.log_file.empty() && !rslog::SetLogFile(opt.log_file.c_str()))
    {
        fprintf(stderr, "错误: 无法写入日志文件 %s\n", opt.log_file.c_str());
        return 1;
    }

    RSLOG_INFO(g_app_log_id, "======== RTSP 推流服务器启动 ========");
    RSLOG_INFO(g_app_log_id, "rtp_packet  版本: %s", rtp_packet_version_string());
    RSLOG_INFO(g_app_log_id, "rtsp_forward 版本: %s", rtsp_forward_version_string());
    RSLOG_INFO(g_app_log_id, "码流文件: %s | 端口: %d | 路径: /%s | 帧率: %d fps",
               opt.file.c_str(), opt.port, opt.rtsp_path.c_str(), opt.fps);

    // 加载并解析码流
    std::vector<uint8_t> data;
    if (LoadFile(opt.file, &data) != 0)
    {
        return 1;
    }

    std::vector<FrameSpan> frames;
    ParamSets params;
    if (BuildAccessUnits(data, &frames, &params) != 0 || frames.empty())
    {
        return 1;
    }

    // 创建 RTSP 服务器
    RtspForwardConfig config = {};
    config.port = opt.port;
    config.ip = "0.0.0.0";
    config.max_sessions = opt.max_sessions;
    config.buffer_size = 65536;
    config.sdp_content = NULL;
    config.connection_timeout_sec = 30;
    config.session_timeout_sec = 60;

    int ret = rtsp_forward_create(&g_server, &config);
    if (ret != RTSP_OK)
    {
        RSLOG_ERROR(g_app_log_id, "创建 RTSP 服务器失败: %d", ret);
        return 1;
    }

    std::string sdp = BuildSdp(params);
    ret = rtsp_forward_set_sdp(g_server, sdp.c_str());
    if (ret != RTSP_OK)
    {
        RSLOG_ERROR(g_app_log_id, "设置 SDP 失败: %d", ret);
        rtsp_forward_destroy(g_server);
        return 1;
    }

    ret = rtsp_forward_start(g_server);
    if (ret != RTSP_OK)
    {
        RSLOG_ERROR(g_app_log_id, "启动 RTSP 服务器失败: %d（端口 %d 是否被占用？）",
                    ret, opt.port);
        rtsp_forward_destroy(g_server);
        return 1;
    }

    // 注册信号处理
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = OnSignal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    signal(SIGPIPE, SIG_IGN);

    // 启动推流
    Streamer streamer(opt, data, frames);
    if (!streamer.Start())
    {
        rtsp_forward_stop(g_server);
        rtsp_forward_run(g_server);
        rtsp_forward_destroy(g_server);
        return 1;
    }

    RSLOG_INFO(g_app_log_id, "服务器就绪，播放地址: rtsp://<ip>:%d/%s", opt.port,
               opt.rtsp_path.c_str());
    RSLOG_INFO(g_app_log_id, "按 Ctrl+C 停止");

    // 主线程阻塞在事件循环
    ret = rtsp_forward_run(g_server);

    // 收尾
    g_running.store(false);
    streamer.Stop();
    rtsp_forward_destroy(g_server);
    g_server = NULL;

    RSLOG_INFO(g_app_log_id, "服务器已停止（事件循环返回 %d）", ret);
    rslog::Flush();
    return (ret == RTSP_OK) ? 0 : 1;
}
