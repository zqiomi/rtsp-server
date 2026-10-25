# 贡献指南

## 环境要求

| 项 | 要求 |
|----|------|
| 操作系统 | Linux（`rtsp-forward` 依赖 epoll，Windows/macOS 无法构建） |
| 编译器 | 支持 C++11 的 GCC / Clang（已验证 GCC 13.3） |
| CMake | >= 3.13 |
| 测试 | ffmpeg / ffprobe（仅跑测试时需要） |

Windows 用户建议使用 WSL2：

```powershell
wsl --install -d Ubuntu-24.04
wsl -u root apt-get install -y cmake g++ ffmpeg
```

## 构建与测试

```bash
git clone <repo-url> rtsp-server
cd rtsp-server

./scripts/build.sh                # RelWithDebInfo
./test/run_test.sh                # 端到端测试
```

提交前请确保：

1. `./scripts/build.sh` 通过（`-Werror` 全开，任何警告都会导致失败）
2. `./test/run_test.sh` 14 项全部 PASS

## 目录约定

```
src/log/            自包含的基础设施（日志）
src/rtp-packet/     可独立链接的封包库
src/rtsp-forward/   可独立链接的转发库
src/app/            可执行程序，只做编排，不放可复用逻辑
```

新增代码时：

- **可复用逻辑放库目录**，应用层只负责参数解析、生命周期、策略选择
- **库之间不允许横向依赖**，`rtp-packet` 不应知道 `rtsp-forward` 的存在
- **公共头文件放 `include/`**，内部实现头放 `src/`，内部头不出现在公共 include 路径中

## 代码风格

格式由 `.clang-format` 定义（基于 Google 风格，4 空格缩进，指针靠类型）：

```bash
clang-format -i src/**/*.cc src/**/*.h
```

约定：

- 命名空间：模块名小写下划线（`rtp_packet`、`rtsp_forward`）
- 类型：`PascalCase`；函数：`PascalCase`（模块内部）；对外 C API：模块前缀小写下划线（`rtsp_forward_create`）
- 成员变量：`snake_case_` 带尾下划线
- 注释用中文，公共 API 用 Doxygen 风格（`@brief` / `@param` / `@return`）
- 日志使用 `LOG_INFO` 等宏，不要直接 `printf`（应用层初始化阶段除外）

## 提交规范

采用 Conventional Commits，与仓库历史保持一致：

```
<type>: <简短描述>

<可选正文>
```

| type | 用途 |
|------|------|
| `feat` | 新功能 |
| `fix` | 缺陷修复 |
| `refactor` | 不改变行为的重构 |
| `perf` | 性能优化 |
| `docs` | 文档 |
| `test` | 测试 |
| `build` | 构建系统、依赖 |
| `chore` | 杂项 |

示例：

```
fix: 修正 AU 切分时 SEI 归属错误的帧

SEI 出现在 VCL 之后时应归属下一帧，原逻辑会把它并入当前帧，
导致该帧时间戳覆盖范围偏大。
```

## 提交前自查

- [ ] 编译零警告（`-Werror`）
- [ ] `test/run_test.sh` 全绿
- [ ] 公共 API 变更同步更新 `include/` 注释与 `docs/`
- [ ] 行为变更写入 `CHANGELOG.md` 的 `[Unreleased]`
- [ ] 没有提交 `build/`、`test/data/`、`test/output/` 等产物

## 分支与 PR

- 主分支 `main` 保持可构建、可测试
- 功能分支命名 `feat/xxx`、`fix/xxx`
- PR 描述中说明：改动动机、验证方式、对现有 API 的影响

## 许可证

提交代码即表示同意以 [MIT](LICENSE) 协议发布。
