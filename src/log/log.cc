/**
 * @file log.cc
 * @brief 轻量级日志库实现
 */

#include "log/log.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#include <atomic>
#include <mutex>

#if defined(__linux__)
#include <sys/syscall.h>
#include <unistd.h>
#endif

namespace rslog
{
namespace
{

std::mutex g_registry_mutex;                     /**< 保护模块注册表 */
const char* g_module_names[kMaxModules] = {0};   /**< 模块名，索引即 ID */
std::atomic<int> g_module_levels[kMaxModules];   /**< 模块级别，-1 表示跟随全局 */
std::atomic<int> g_global_level(kInfo);          /**< 全局级别 */
std::atomic<int> g_module_count(1);              /**< 下一个可用 ID */

std::mutex g_output_mutex;                       /**< 保护输出流 */
FILE* g_out = stdout;                            /**< 当前输出流 */
bool g_out_owned = false;                        /**< 是否由本库打开（需关闭） */
std::atomic<bool> g_color(false);                /**< 是否启用彩色 */

/**
 * @brief 初始化模块级别数组（-1 表示跟随全局）
 */
struct LevelInit
{
    LevelInit()
    {
        for (int i = 0; i < kMaxModules; ++i)
        {
            g_module_levels[i].store(-1);
        }
    }
};
LevelInit g_level_init;

/**
 * @brief 取文件名部分（去掉目录）
 */
const char* BaseName(const char* path)
{
    if (path == NULL)
    {
        return "";
    }
    const char* slash = strrchr(path, '/');
    const char* back = strrchr(path, '\\');
    if (back != NULL && (slash == NULL || back > slash))
    {
        slash = back;
    }
    return (slash != NULL) ? slash + 1 : path;
}

/**
 * @brief 当前线程 ID
 */
unsigned long CurrentThreadId()
{
#if defined(__linux__)
    return static_cast<unsigned long>(::syscall(SYS_gettid));
#else
    static std::atomic<unsigned long> next(1);
    static thread_local unsigned long tid = 0;
    if (tid == 0)
    {
        tid = next.fetch_add(1);
    }
    return tid;
#endif
}

/**
 * @brief 各基本级别的终端颜色
 */
const char* LevelColor(Level level)
{
    switch (level)
    {
        case kTrace:
        case kDebug:
            return "\033[36m";  // 青色
        case kInfo:
            return "\033[32m";  // 绿色
        case kWarn:
            return "\033[33m";  // 黄色
        case kError:
        case kFatal:
            return "\033[31m";  // 红色
        default:
            return "";
    }
}

/**
 * @brief 格式化时间戳：YYYY-MM-DD HH:MM:SS.mmm
 */
void FormatTimestamp(char* buf, size_t size)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);

    struct tm tm_now;
    localtime_r(&ts.tv_sec, &tm_now);

    char base[32];
    if (strftime(base, sizeof(base), "%Y-%m-%d %H:%M:%S", &tm_now) == 0)
    {
        snprintf(base, sizeof(base), "0000-00-00 00:00:00");
    }

    snprintf(buf, size, "%s.%03ld", base, static_cast<long>(ts.tv_nsec / 1000000));
}

}  // namespace

int RegisterModule(const char* name)
{
    if (name == NULL || name[0] == '\0')
    {
        return 0;
    }

    std::lock_guard<std::mutex> lock(g_registry_mutex);

    // 同名模块只注册一次
    for (int i = 1; i < kMaxModules; ++i)
    {
        if (g_module_names[i] != NULL && strcmp(g_module_names[i], name) == 0)
        {
            return i;
        }
    }

    int id = g_module_count.fetch_add(1);
    if (id >= kMaxModules)
    {
        g_module_count.store(kMaxModules);
        return 0;
    }

    g_module_names[id] = name;
    return id;
}

void SetModuleLevel(int id, Level level)
{
    if (id <= 0 || id >= kMaxModules)
    {
        return;
    }
    g_module_levels[id].store(static_cast<int>(level));
}

void SetGlobalLevel(Level level)
{
    g_global_level.store(static_cast<int>(level));
}

Level GetGlobalLevel()
{
    return static_cast<Level>(g_global_level.load());
}

Level GetModuleLevel(int id)
{
    if (id > 0 && id < kMaxModules)
    {
        int lv = g_module_levels[id].load();
        if (lv >= 0)
        {
            return static_cast<Level>(lv);
        }
    }
    return GetGlobalLevel();
}

bool ShouldLog(int id, Level level)
{
    int threshold = (id > 0 && id < kMaxModules) ? g_module_levels[id].load() : -1;
    if (threshold < 0)
    {
        threshold = g_global_level.load();
    }
    return static_cast<int>(level) >= threshold;
}

bool SetLogFile(const char* path)
{
    std::lock_guard<std::mutex> lock(g_output_mutex);

    if (path == NULL || path[0] == '\0')
    {
        if (g_out_owned && g_out != NULL)
        {
            fclose(g_out);
        }
        g_out = stdout;
        g_out_owned = false;
        g_color.store(false);
        return true;
    }

    FILE* fp = fopen(path, "a");
    if (fp == NULL)
    {
        return false;
    }

    if (g_out_owned && g_out != NULL)
    {
        fclose(g_out);
    }
    g_out = fp;
    g_out_owned = true;
    g_color.store(false);  // 文件输出不做颜色标记
    return true;
}

void SetColorEnabled(bool enabled)
{
    g_color.store(enabled);
}

void Flush()
{
    std::lock_guard<std::mutex> lock(g_output_mutex);
    if (g_out != NULL)
    {
        fflush(g_out);
    }
}

const char* LevelName(Level level)
{
    switch (level)
    {
        case kTrace:
            return "TRACE";
        case kDebug:
            return "DEBUG";
        case kInfo:
            return "INFO";
        case kWarn:
            return "WARN";
        case kError:
            return "ERROR";
        case kFatal:
            return "FATAL";
        default:
            return "OFF";
    }
}

bool ParseLevel(const char* str, Level* out)
{
    if (str == NULL || out == NULL)
    {
        return false;
    }

    static const struct
    {
        const char* name;
        Level level;
    } kTable[] = {
        {"trace", kTrace}, {"debug", kDebug}, {"info", kInfo}, {"warn", kWarn},
        {"warning", kWarn}, {"error", kError}, {"fatal", kFatal}, {"off", kOff},
    };

    for (size_t i = 0; i < sizeof(kTable) / sizeof(kTable[0]); ++i)
    {
        if (strcasecmp(str, kTable[i].name) == 0)
        {
            *out = kTable[i].level;
            return true;
        }
    }
    return false;
}

void Write(int id, Level level, const char* file, int line, const char* func,
           const char* fmt, ...)
{
    char timestamp[64];
    FormatTimestamp(timestamp, sizeof(timestamp));

    const char* module = "app";
    if (id > 0 && id < kMaxModules && g_module_names[id] != NULL)
    {
        module = g_module_names[id];
    }

    // 消息体：栈上缓冲，超长截断，避免热点路径动态分配
    char message[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);

    std::lock_guard<std::mutex> lock(g_output_mutex);
    if (g_out == NULL)
    {
        return;
    }

    bool color = g_color.load();
    const char* head_color = color ? LevelColor(level) : "";
    const char* tail_color = (color && head_color[0] != '\0') ? "\033[0m" : "";

    fprintf(g_out, "%s[%s][%-5s][%s][%lu][%s:%d %s] %s%s\n",
            head_color, timestamp, LevelName(level), module, CurrentThreadId(),
            BaseName(file), line, (func != NULL) ? func : "-", message, tail_color);

    // 错误以上立即落盘，避免崩溃时丢失现场
    if (level >= kWarn)
    {
        fflush(g_out);
    }
}

}  // namespace rslog
