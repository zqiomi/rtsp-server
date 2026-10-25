# Changelog

本项目所有值得注意的变更都记录在此文件。

格式遵循 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/)，
版本号遵循 [语义化版本](https://semver.org/lang/zh-CN/)。

## [Unreleased]

### 计划中

- RTCP 接收端报告（RR）与 NACK 重传
- 多路流并发（当前单进程单路源）
- 配置文件支持
- RTSP 鉴权（Basic / Digest）
- H.265 输入（封包层已支持，应用层待接入）

## [1.0.0] - 2026-10-25

首个版本。

### 新增

- **RTSP 推流服务器**：读取 H.264 Annex-B 码流，按目标帧率封包为 RTP，通过 RTSP 同时转发给多个客户端
- **`rslog` 轻量日志库**：模块注册、5 级过滤、毫秒时间戳、线程 ID、终端/文件输出、按模块独立设置级别
- **RTP 封包层**：H.264/H.265 支持，RFC 6184 single NAL / FU-A 分片，SPS/PPS 缓存与自动插入，RTCP SR 构造
- **RTSP 转发层**：epoll 事件循环、会话状态机、环形缓冲区、一路输入多路转发
- **应用层**：
  - 完整命令行参数（端口、帧率、路径、MTU、SSRC、日志级别、循环播放）
  - 按访问单元（AU）切帧，同帧多 NALU 共享 RTP 时间戳
  - 从码流提取 SPS/PPS 写入 SDP 的 `sprop-parameter-sets` 与 `profile-level-id`
  - 基于绝对时间截止点的帧率调度，长时间运行不累积漂移
  - 周期性输出会话与发包统计
  - SIGINT/SIGTERM 优雅退出
- **端到端测试 `test/run_test.sh`**：14 项断言，覆盖 ffprobe 流校验、5 秒拉流解码、双客户端并发、优雅退出
- **构建脚本 `scripts/build.sh`**：一键配置与并行编译
- **文档**：架构说明、日志库说明、贡献指南、本变更日志

### 技术要点

- 依赖方向严格单向：`app → rtp-packet / rtsp-forward → log`
- 构建选项由顶层统一定义，子模块只声明 target，不注册独立打包规则
- 统一编译选项 `-Wall -Wextra -Werror -fno-exceptions`，默认 `RelWithDebInfo`
- 非 Linux 平台在 CMake 配置阶段直接报错并提示方案

### 已知限制

- 仅支持 Linux（转发层基于 epoll）
- 单路流：一个进程只服务一路码流，所有客户端共享同一 SDP
- 客户端在 GOP 中间接入时，需等到下一个 IDR 才能出图（GOP 25 @25fps 约 1 秒）
- 无 RTCP RR / NACK 重传，丢包不可恢复
- 无鉴权
