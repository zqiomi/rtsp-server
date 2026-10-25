# 架构说明

## 分层

```
┌───────────────────────────────────────────────┐
│  src/app                     应用层           │
│  CLI 解析 · 码流加载 · AU 切帧 · 帧率调度     │
│  统计输出 · 信号处理                          │
└───────────────┬───────────────────────────────┘
                │ 调用 C API
┌───────────────▼───────────────┐  ┌──────────────┐
│  src/rtp-packet               │  │              │
│  H.264/H.265 → RTP 封包       │  │   src/log    │
│  RFC 6184 single/FU-A 分片    │  │    rslog     │
│  SPS/PPS 缓存与自动插入       │  │              │
│  RTCP SR 构造                 │  │              │
└───────────────┬───────────────┘  └──────┬───────┘
                │ 输出 RTP 包             │
┌───────────────▼─────────────────────────▼───────┐
│  src/rtsp-forward                               │
│  epoll 事件循环 · RTSP 信令 · 会话状态机        │
│  环形缓冲区 · 一路输入多路转发                  │
└─────────────────────────────────────────────────┘
```

依赖方向严格单向：`app → rtp-packet / rtsp-forward → log`。两个库互不依赖，`rtp-packet` 不知道 `rtsp-forward` 的存在，二者只在应用层被组装。

## 数据流

```
H.264 文件
   │  一次性读入内存（测试用例 1.2 MB / 250 帧）
   ▼
SplitNalus            按 0x000001 起始码切出 NALU
   │
   ▼
BuildAccessUnits      按 VCL 边界合并为访问单元
   │                  同时提取首组 SPS/PPS
   ▼
pack_frame(AU, ts)    一帧调用一次，同帧 NALU 共享时间戳
   │
   ▼
RTP 包回调            → rtsp_forward_send_rtp()
   │
   ▼
所有 PLAYING 会话      环形缓冲 → TCP/UDP 发送
```

## 线程模型

| 线程 | 职责 | 阻塞点 |
|------|------|--------|
| 主线程 | RTSP 信令、会话管理 | `rtsp_forward_run()` 内的 `epoll_wait` |
| 推流线程 | 读帧、封包、发送 RTP | 帧间隔 `sleep_for` |

两线程共享一个 `RtspForward` 句柄，`rtsp_forward_send_rtp()` 是线程安全的（内部加锁写入各会话的环形缓冲区）。

退出路径：信号处理函数置 `g_running=false` 并调用 `rtsp_forward_stop()` 唤醒事件循环 → 主线程 `run()` 返回 → 置标志、`join` 推流线程 → `rtsp_forward_destroy()`。

## 关键设计决策

### 按访问单元切帧，而不是按 NALU

RTP 的时间戳语义是"一个时间戳对应一个呈现时刻"，也就是一帧。若对每个 NALU 分别调用 `pack_frame` 并各自递增时间戳，等于把 SPS/PPS/IDR 当成三个不同时刻的画面，客户端按 RTP 时间戳推算的帧率就会失真。

切分规则：

- 遇到 VCL NALU（类型 1-5），且当前 AU 已有 VCL → 开始新 AU
- 遇到 SPS/PPS/SEI/AUD（类型 6-9），且当前 AU 已有 VCL → 归属下一 AU，开始新 AU

实测 10 秒 / 25fps / GOP 25 的码流解析出 **271 个 NALU → 250 帧**，帧数与时长完全吻合。

### SDP 携带参数集

`pack_frame` 内部会缓存 SPS/PPS，并在 IDR 缺少参数集时自动补插。但客户端在收到第一个 IDR 之前仍会报 `non-existing PPS 0 referenced`。

因此应用层额外从码流中提取首组 SPS/PPS，写入 SDP：

```
a=fmtp:96 packetization-mode=1;profile-level-id=42C01E;sprop-parameter-sets=...,...
```

效果：拉流 stderr 从满屏 PPS 报错变为完全为空。

### 帧率调度用绝对截止时间

逐帧 `sleep(帧间隔)` 会把每次调度误差累积起来，跑几分钟就明显偏慢。现在维护一个 `deadline_us` 累加器：

```cpp
deadline_us += frame_interval_us;      // 绝对目标时刻
if (now < deadline_us) sleep(deadline_us - now);
else if (now - deadline_us > frame_interval_us) deadline_us = now;  // 落后超一帧则重新对齐
```

落后超过一帧说明系统被卡住（比如磁盘抖动），此时放弃追帧直接对齐，避免后续连发。

### 自包含的日志层

日志只服务本工程的三个使用方（封包库、转发库、应用），因此实现得尽量薄：约 500 行的 `rslog`，保留真正用到的能力——

- 按模块独立设置级别（运行期可调）
- 毫秒时间戳、级别、模块名、线程 ID、源位置
- 终端彩色输出 / 文件输出

不做的事：网络远程配置、按模块重定向到不同文件、异步落盘。这些都带不来实际收益，只会增加维护面。详见 [logging.md](logging.md)。

所有库和应用共用一个 `src/log/module_log.h`，它只做一件事：把 `rslog` 绑定到当前编译目标的模块名。每个库在 CMake 里定义两个宏——命名空间与模块名——头文件据此生成 `GetLogModuleId()` 并展开 `LOG_INFO` 等宏。库内部只写 `LOG_INFO(...)`，更换日志实现只需改这一个头文件；两个库用不同命名空间，链接到一起也不会互相污染模块 ID。

### 构建系统的组织

- **顶层唯一定义选项**（`BUILD_SHARED_LIBS` / `BUILD_STATIC_LIBS` / `BUILD_APP` / `ENABLE_WERROR`），子目录不再声明 `option()`
- 子目录显式 `add_library(... SHARED/STATIC ...)`，不依赖 `BUILD_SHARED_LIBS` 的隐式行为，避免选项在多层嵌套间相互影响
- 每个库提供一个 `configure_target()` 函数统一设置 include、编译选项、链接和版本宏
- 每个库导出 `rtp_packet` / `rtsp_forward` 别名，上层不关心链接的是静态库还是动态库
- 版本号通过 `target_compile_definitions` 注入，不生成中间头文件

## 已知限制与根因

| 限制 | 根因 | 可行的解法 |
|------|------|-----------|
| 客户端中途接入需等下一 IDR | RTSP 层不感知"会话开始播放"这一事件 | 在 `rtsp-forward` 增加会话开始回调，让应用层立即重发 SPS/PPS+IDR |
| 单路流 | 应用层只创建了一个 packer 和一个 SDP | 引入"媒体源"抽象，按路径映射到不同 packer |
| 丢包不可恢复 | 未实现 RTCP RR/NACK | 在 `rtsp-forward` 增加 RTCP 通道与重传缓存 |
| 仅 Linux | `epoll` 强绑定 | 抽象事件循环接口，补 kqueue/IOCP 实现 |

## 扩展指引

**接入 H.265**：封包层 `rtp_packer_create` 已支持 `RTP_CODEC_H265`，应用层只需把 `config.codec` 换成 H.265、SDP 的 `rtpmap` 改为 `H265/90000`，并按 H.265 的 NAL 类型规则调整 AU 切分（类型 0-31 为 VCL，32-34 为 VPS/SPS/PPS）。

**多路流**：把当前 `Streamer` 中的 packer 与帧数据提取为独立对象，用 `std::map<std::string, Streamer*>` 按 RTSP 路径分发；`rtsp-forward` 侧需要按路径区分会话（当前忽略路径）。
