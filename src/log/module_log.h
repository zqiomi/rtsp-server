/**
 * @file module_log.h
 * @brief 库内日志宏：把 rslog 绑定到当前编译目标的模块名
 *
 * 每个库在 CMake 中定义两个宏：
 *   RS_LOG_MODULE_NS   —— 存放模块 ID 访问器的命名空间（如 rtp_packet）
 *   RS_LOG_MODULE_NAME —— 注册到 rslog 的模块名（如 "rtp_packet"）
 *
 * 同一个库的所有编译单元使用同一组定义，生成的 inline 函数满足 ODR 要求；
 * 不同库使用不同命名空间，即使链接到一起也互不冲突。
 *
 * 用法：库内源文件直接 #include "log/module_log.h"，然后写 LOG_INFO(...)。
 */

#ifndef RSLOG_MODULE_LOG_H_
#define RSLOG_MODULE_LOG_H_

#include "log/log.h"

#if !defined(RS_LOG_MODULE_NS) || !defined(RS_LOG_MODULE_NAME)
#error "缺少 RS_LOG_MODULE_NS / RS_LOG_MODULE_NAME 定义，请在 CMakeLists.txt 中通过 target_compile_definitions 设置"
#endif

namespace RS_LOG_MODULE_NS
{

/**
 * @brief 获取本库的日志模块 ID
 *
 * 首次调用时注册，之后返回同一 ID。
 */
inline int GetLogModuleId()
{
    static int id = ::rslog::RegisterModule(RS_LOG_MODULE_NAME);
    return id;
}

}  // namespace RS_LOG_MODULE_NS

#define LOG_TRACE(fmt, ...) \
    RSLOG_TRACE(::RS_LOG_MODULE_NS::GetLogModuleId(), fmt, ##__VA_ARGS__)

#define LOG_DEBUG(fmt, ...) \
    RSLOG_DEBUG(::RS_LOG_MODULE_NS::GetLogModuleId(), fmt, ##__VA_ARGS__)

#define LOG_INFO(fmt, ...) \
    RSLOG_INFO(::RS_LOG_MODULE_NS::GetLogModuleId(), fmt, ##__VA_ARGS__)

#define LOG_WARN(fmt, ...) \
    RSLOG_WARN(::RS_LOG_MODULE_NS::GetLogModuleId(), fmt, ##__VA_ARGS__)

#define LOG_ERROR(fmt, ...) \
    RSLOG_ERROR(::RS_LOG_MODULE_NS::GetLogModuleId(), fmt, ##__VA_ARGS__)

#define LOG_FATAL(fmt, ...) \
    RSLOG_FATAL(::RS_LOG_MODULE_NS::GetLogModuleId(), fmt, ##__VA_ARGS__)

#endif  // RSLOG_MODULE_LOG_H_
