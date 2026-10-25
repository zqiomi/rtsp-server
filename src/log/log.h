/**
 * @file log.h
 * @brief 轻量级日志库（自包含，无外部依赖）
 *
 * 设计目标：
 * 1. 零外部依赖，仅依赖 C++11 标准库与 POSIX
 * 2. 支持多模块独立级别控制（每个库/应用可作为独立模块注册）
 * 3. printf 风格格式化，热点路径避免内存分配
 * 4. 单行原子输出，多线程下不交错
 *
 * 典型用法：
 * @code
 * // 模块注册（通常每个库注册一次）
 * static int g_mod = rslog::RegisterModule("rtsp_forward");
 *
 * // 输出日志
 * RSLOG_INFO(g_mod, "server started on port %d", port);
 *
 * // 运行期调整级别
 * rslog::SetModuleLevel(g_mod, rslog::kDebug);
 * @endcode
 */

#ifndef RSLOG_LOG_H_
#define RSLOG_LOG_H_

#include <stdarg.h>
#include <stddef.h>

namespace rslog
{

/**
 * @brief 日志级别
 *
 * 数值越大越严重。kOff 用于关闭某个模块的全部输出。
 */
enum Level
{
    kTrace = 0, /**< 最详细的跟踪信息 */
    kDebug = 1, /**< 调试信息 */
    kInfo = 2,  /**< 常规运行信息（默认级别） */
    kWarn = 3,  /**< 警告，不影响继续运行 */
    kError = 4, /**< 错误，功能受损 */
    kFatal = 5, /**< 致命错误 */
    kOff = 6,   /**< 关闭输出（仅用作级别阈值） */
};

/** 支持的最大模块数，超出后注册返回 0（静默丢弃） */
const int kMaxModules = 32;

/**
 * @brief 注册日志模块
 *
 * 线程安全。同名模块重复注册返回同一个 ID。
 *
 * @param[in] name 模块名，需为静态存储期字符串
 * @return 模块 ID（1..kMaxModules-1），失败返回 0
 */
int RegisterModule(const char* name);

/**
 * @brief 设置指定模块的日志级别
 *
 * @param[in] id 模块 ID
 * @param[in] level 级别阈值，低于该级别的日志被丢弃
 */
void SetModuleLevel(int id, Level level);

/**
 * @brief 设置全局默认级别，作用于所有未单独设置级别的模块
 */
void SetGlobalLevel(Level level);

/**
 * @brief 获取全局默认级别
 */
Level GetGlobalLevel();

/**
 * @brief 获取指定模块的当前生效级别
 */
Level GetModuleLevel(int id);

/**
 * @brief 判断某条日志是否需要输出（供宏使用，热点路径）
 */
bool ShouldLog(int id, Level level);

/**
 * @brief 设置日志输出文件
 *
 * @param[in] path 文件路径，传 NULL 或空串则输出到 stdout
 * @return 成功返回 true
 *
 * @note 设置后立即生效，成功打开新文件后关闭旧文件
 */
bool SetLogFile(const char* path);

/**
 * @brief 是否启用彩色输出（仅在输出到终端时生效）
 */
void SetColorEnabled(bool enabled);

/**
 * @brief 刷新输出缓冲区
 */
void Flush();

/**
 * @brief 级别名称（如 "INFO"）
 */
const char* LevelName(Level level);

/**
 * @brief 解析级别名称，支持大小写
 *
 * @param[in] str 名称，如 "debug"
 * @param[out] out 解析结果
 * @return 解析成功返回 true
 */
bool ParseLevel(const char* str, Level* out);

/**
 * @brief 输出一条日志（一般通过宏调用，不直接使用）
 *
 * @param[in] id 模块 ID
 * @param[in] level 级别
 * @param[in] file 源文件（__FILE__）
 * @param[in] line 行号（__LINE__）
 * @param[in] func 函数名（__func__）
 * @param[in] fmt printf 风格格式串
 */
void Write(int id, Level level, const char* file, int line, const char* func,
           const char* fmt, ...)
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((format(printf, 6, 7)))
#endif
    ;

}  // namespace rslog

/**
 * @brief 日志输出宏
 *
 * 先做级别判断再求值参数，低于阈值时零开销。
 */
#define RSLOG_WRITE(id, lv, fmt, ...)                                        \
    do                                                                       \
    {                                                                        \
        if (::rslog::ShouldLog((id), (lv)))                                  \
        {                                                                    \
            ::rslog::Write((id), (lv), __FILE__, __LINE__, __func__, (fmt),  \
                           ##__VA_ARGS__);                                   \
        }                                                                    \
    } while (0)

#define RSLOG_TRACE(id, fmt, ...) RSLOG_WRITE((id), ::rslog::kTrace, fmt, ##__VA_ARGS__)
#define RSLOG_DEBUG(id, fmt, ...) RSLOG_WRITE((id), ::rslog::kDebug, fmt, ##__VA_ARGS__)
#define RSLOG_INFO(id, fmt, ...) RSLOG_WRITE((id), ::rslog::kInfo, fmt, ##__VA_ARGS__)
#define RSLOG_WARN(id, fmt, ...) RSLOG_WRITE((id), ::rslog::kWarn, fmt, ##__VA_ARGS__)
#define RSLOG_ERROR(id, fmt, ...) RSLOG_WRITE((id), ::rslog::kError, fmt, ##__VA_ARGS__)
#define RSLOG_FATAL(id, fmt, ...) RSLOG_WRITE((id), ::rslog::kFatal, fmt, ##__VA_ARGS__)

#endif  // RSLOG_LOG_H_
