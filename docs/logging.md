# 日志库 rslog

`src/log` 提供一套自包含的日志设施，只依赖 C++11 标准库与 POSIX，被两个库和应用共同使用。

## 快速使用

```cpp
#include "log/log.h"

// 1. 注册模块（进程内同名只注册一次，可重复调用）
static int g_log = rslog::RegisterModule("my_module");

// 2. 输出
RSLOG_INFO(g_log, "server started on port %d", port);
RSLOG_WARN(g_log, "buffer usage %zu/%zu", used, cap);
RSLOG_ERROR(g_log, "bind failed: %s", strerror(errno));
```

宏会先判断级别再求值参数，低于阈值的调用零格式化开销。

## 级别

| 枚举 | 数值 | 用途 |
|------|------|------|
| `kTrace` | 0 | 最详细跟踪 |
| `kDebug` | 1 | 调试 |
| `kInfo` | 2 | 常规运行信息（默认） |
| `kWarn` | 3 | 警告，不影响继续运行 |
| `kError` | 4 | 错误，功能受损 |
| `kFatal` | 5 | 致命 |
| `kOff` | 6 | 关闭该模块输出 |

## 配置接口

| 函数 | 说明 |
|------|------|
| `RegisterModule(name)` | 注册模块，返回 ID（1..31），失败返回 0 |
| `SetModuleLevel(id, level)` | 设置某模块阈值，优先级高于全局 |
| `SetGlobalLevel(level)` | 设置全局默认阈值 |
| `GetModuleLevel(id)` | 查询某模块当前生效阈值 |
| `SetLogFile(path)` | 输出到文件（追加），传 `NULL` 恢复终端 |
| `SetColorEnabled(bool)` | 终端彩色输出开关 |
| `Flush()` | 刷新缓冲 |
| `ParseLevel(str, &level)` | 解析 `"debug"` 这类字符串，大小写不敏感 |

### 单独调高某个模块的日志

```cpp
int rtp_log = rtp_packet_get_log_module_id();
int rtsp_log = rtsp_forward_get_log_module_id();

rslog::SetModuleLevel(rtp_log, rslog::kDebug);   // 只看封包细节
rslog::SetModuleLevel(rtsp_log, rslog::kError);  // 转发层只留错误
```

## 输出格式

```
[2026-10-25 16:25:12.210][INFO ][rtsp_server][5688][main.cc:514 BuildAccessUnits] 参数集: SPS 24 字节, PPS 5 字节
```

| 字段 | 说明 |
|------|------|
| 时间戳 | 本地时间，毫秒精度 |
| 级别 | 左对齐 5 字符 |
| 模块 | 注册时传入的名字 |
| 线程 ID | Linux 下为真实 TID（`gettid`） |
| 源位置 | `文件名:行号 函数名` |
| 消息 | 最长 1023 字节，超出截断 |

## 线程安全与性能

- 模块注册表与会话表均加锁，`ShouldLog()` 走原子读，无锁竞争
- 输出持有一把互斥锁，保证单行不交错
- 消息体在栈上 1 KB 缓冲中格式化，热点路径不分配堆内存
- `kWarn` 及以上级别立即 `fflush`，避免崩溃时丢现场；低级别依赖缓冲

## 与各库的关系

所有库和应用共用 **一个** `src/log/module_log.h`，它本身不含日志实现，只负责把 `rslog` 绑定到当前编译目标的模块名。

绑定通过两个编译宏完成，由各库的 CMakeLists 设置：

| 宏 | 示例值 | 作用 |
|----|--------|------|
| `RS_LOG_MODULE_NS` | `rtp_packet` | 生成 `GetLogModuleId()` 的命名空间 |
| `RS_LOG_MODULE_NAME` | `"rtp_packet"` | 注册到 `rslog` 的模块名 |

头文件展开后等价于：

```cpp
namespace rtp_packet {
inline int GetLogModuleId()
{
    static int id = ::rslog::RegisterModule("rtp_packet");
    return id;
}
}
#define LOG_INFO(fmt, ...) RSLOG_INFO(::rtp_packet::GetLogModuleId(), fmt, ##__VA_ARGS__)
```

为什么用命名空间而不是一个全局函数：两个库链接到同一个进程，若共用一个 `GetLogModuleId()` 就会出现 ODR 冲突，链接器只保留一份实现，两个库会被注册成同一个模块、共用一个日志级别。命名空间让每个库生成各自的 inline 函数，互不干扰。

实测运行日志中三个来源的标签是分开的：`[rtp_packet]`、`[rtsp_forward]`、`[rtsp_server]`。

因此库内部只写 `LOG_INFO(...)`，不直接依赖 `RSLOG_*` 宏；将来更换日志实现，只需改这一个头文件。

对外暴露的 `rtp_packet_get_log_module_id()` / `rtsp_forward_get_log_module_id()` 返回的就是 `rslog` 的模块 ID，调用方拿到后可直接用 `rslog::SetModuleLevel()` 控制。
