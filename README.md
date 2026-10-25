# rtsp-server

轻量级 **RTSP 推流服务器**，读取 H.264 Annex-B 码流文件，按目标帧率封包为 RTP，通过 RTSP 协议同时转发给多个客户端。

## 特性

- **自包含**：日志、封包、转发全部在仓库内，构建时无需联网
- **一个输入、多路输出**：同一路码流可同时被多个 RTSP 客户端播放
- **按访问单元(AU)封包**：同一帧的多个 NALU 共享一个 RTP 时间戳，符合 RFC 6184 语义
- **自动 SPS/PPS**：IDR 帧缺少参数集时自动补发缓存的 SPS/PPS，客户端可从任意时刻接入
- **绝对时间调度**：按微秒级截止时间推帧，长时间运行不累积漂移
- **模块化日志**：`rslog` 支持按模块独立控制级别，可输出到终端或文件
- **优雅退出**：SIGINT/SIGTERM 停止事件循环、回收线程、释放资源

## 目录结构

```
rtsp-server/
├── src/
│   ├── log/             轻量日志库 rslog（自包含）
│   ├── rtp-packet/      RTP 封包（H.264/H.265 → RFC 6184）
│   ├── rtsp-forward/    RTSP 信令 + 多路转发（epoll 事件循环）
│   └── app/             应用层（码流解析、帧率控制、会话统计）
├── docs/
│   ├── architecture.md  架构、线程模型、设计决策与已知限制
│   └── logging.md       rslog 使用与配置
├── scripts/build.sh     构建脚本
├── test/run_test.sh     ffmpeg 端到端测试
├── CHANGELOG.md         版本变更记录
├── CONTRIBUTING.md      贡献指南
└── LICENSE              MIT
```

`rtp-packet` 与 `rtsp-forward` 是可单独链接的库，各自维护 `include/`（对外）与 `src/`（内部）分层。依赖方向严格单向：`app → rtp-packet / rtsp-forward → log`。

## 依赖

| 依赖 | 版本 | 说明 |
|------|------|------|
| CMake | >= 3.13 | 构建系统 |
| C++ 编译器 | 支持 C++11 | GCC / Clang |
| pthread | - | 线程 |
| **操作系统** | **Linux** | `rtsp-forward` 基于 epoll，Windows/macOS 无法构建 |
| ffmpeg / ffprobe | - | 仅测试需要 |

## 构建

```bash
./scripts/build.sh              # RelWithDebInfo
./scripts/build.sh Debug        # Debug
./scripts/build.sh Release clean # 清理重建
```

或手动：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j$(nproc)
```

产物：`build/bin/rtsp_server`

### 构建选项

| 选项 | 默认 | 说明 |
|------|------|------|
| `BUILD_SHARED_LIBS` | ON | 构建动态库 |
| `BUILD_STATIC_LIBS` | ON | 构建静态库 |
| `BUILD_APP` | ON | 构建 rtsp_server |
| `ENABLE_WERROR` | ON | 警告视为错误 |
| `ENABLE_INSTALL` | OFF | 生成安装规则 |

## 运行

```bash
# 准备一路 H.264 码流
ffmpeg -f lavfi -i testsrc2=duration=10:size=640x480:rate=25 \
       -c:v libx264 -profile:v baseline -g 25 -bf 0 -f h264 test.h264

# 启动服务器
./build/bin/rtsp_server -f test.h264 -p 8554 --fps 25

# 播放（任选其一）
ffplay -rtsp_transport tcp rtsp://127.0.0.1:8554/stream
vlc rtsp://127.0.0.1:8554/stream
```

### 命令行参数

| 参数 | 默认 | 说明 |
|------|------|------|
| `-f, --file` | 必填 | H.264 Annex-B 码流文件 |
| `-p, --port` | 8554 | 监听端口 |
| `--path` | stream | RTSP 路径名（与库内路由无关，仅用于拼接 URL） |
| `--fps` | 25 | 推流帧率 |
| `--max-sessions` | 10 | 最大并发会话 |
| `--mtu` | 1400 | RTP MTU，超过则 FU-A 分片 |
| `--ssrc` | 0x12345678 | RTP SSRC |
| `--no-loop` | 关 | 播完不循环（默认循环） |
| `--log-level` | info | trace/debug/info/warn/error |
| `--log-file` | - | 日志写入文件 |

## 测试

```bash
./test/run_test.sh          # 默认端口 8554
./test/run_test.sh 9554     # 指定端口
```

测试内容（全部基于 ffmpeg/ffprobe，无 mock）：

1. ffmpeg 生成 10 秒 640x480@25fps H.264 码流
2. 启动服务器并验证端口监听、码流解析结果
3. ffprobe 校验拉流编码为 h264、分辨率 640x480
4. ffmpeg 拉流 5 秒，校验解码帧数 ≥100 且整段解码无错误，抽帧存图
5. 两路客户端并发拉流，校验均成功并检查服务端会话统计
6. SIGINT 优雅退出，检查退出日志与崩溃痕迹

产物在 `test/output/`。

## 文档

| 文档 | 内容 |
|------|------|
| [docs/architecture.md](docs/architecture.md) | 分层与数据流、线程模型、关键设计决策、已知限制根因、扩展指引 |
| [docs/logging.md](docs/logging.md) | rslog 的 API、级别控制、输出格式、性能考量 |
| [CHANGELOG.md](CHANGELOG.md) | 版本变更记录 |
| [CONTRIBUTING.md](CONTRIBUTING.md) | 环境要求、构建测试、代码风格、提交规范 |

## 已知限制

| 限制 | 说明 |
|------|------|
| 仅 Linux | `rtsp-forward` 基于 epoll |
| 单路流 | 一个进程只服务一路码流，所有客户端共享同一 SDP |
| 中途接入需等 IDR | 客户端在 GOP 中间接入时，要等下一个关键帧才出图（GOP 25 @25fps 约 1 秒），这是 RTSP 层缺少"会话开始"回调导致的，根因与解法见架构文档 |
| 无丢包恢复 | 未实现 RTCP RR / NACK 重传 |
| 无鉴权 | 任何能连上端口的客户端都可播放 |

## 后续可扩展

- RTCP 接收端报告（RR）与 NACK 重传
- 多路流并发（`rtsp_forward` 已支持多会话，应用层可扩展为多路源）
- 配置文件取代命令行参数
- 鉴权（Basic/Digest）
- H.265 码流（`rtp-packet` 已支持，应用层加 `--codec h265` 即可）

## License

MIT
