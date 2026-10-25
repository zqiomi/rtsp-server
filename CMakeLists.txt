# ============================================================
# rtsp-server —— 单仓库 RTSP 推流服务器
#
# 目录结构：
#   src/log            轻量日志库（自包含，无外部依赖）
#   src/rtp-packet     RTP 封包（H.264/H.265）
#   src/rtsp-forward   RTSP 信令与多路转发（epoll）
#   src/app            应用层
#
# 构建：
#   cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
#   cmake --build build -j
# ============================================================

cmake_minimum_required(VERSION 3.13)

project(rtsp-server
    VERSION 1.0.0
    DESCRIPTION "RTSP 推流服务器（H.264 over RTP/RTSP）"
    LANGUAGES CXX)

# ===== 平台检查 =====
# rtsp_forward 基于 epoll 实现，仅支持 Linux
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR
        "本项目依赖 epoll，仅支持在 Linux 上构建。\n"
        "  当前系统: ${CMAKE_SYSTEM_NAME}\n"
        "  可选方案: 使用 WSL2 / Docker / 原生 Linux 主机。")
endif()

# ===== 语言标准 =====
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 未指定构建类型时默认带调试符号的 Release
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "构建类型" FORCE)
endif()

# ===== 构建选项 =====
# 注意：BUILD_SHARED_LIBS 是 CMake 内建变量，此处作为统一开关使用。
# 模块内部显式声明了 SHARED/STATIC，不依赖该变量的隐式行为。
option(BUILD_SHARED_LIBS "构建动态库" ON)
option(BUILD_STATIC_LIBS "构建静态库" ON)
option(BUILD_APP "构建 rtsp_server 可执行文件" ON)
option(ENABLE_WERROR "把编译警告视为错误" ON)
option(ENABLE_INSTALL "生成安装规则" OFF)

if(NOT BUILD_SHARED_LIBS AND NOT BUILD_STATIC_LIBS)
    message(FATAL_ERROR "BUILD_SHARED_LIBS 与 BUILD_STATIC_LIBS 至少开启一个")
endif()

# ===== 统一编译选项 =====
set(RS_MODULE_COMPILE_OPTIONS -Wall -Wextra)

if(ENABLE_WERROR)
    list(APPEND RS_MODULE_COMPILE_OPTIONS -Werror)
endif()

# 模块层保持无异常（与原有约定一致）
set(RS_MODULE_COMPILE_OPTIONS ${RS_MODULE_COMPILE_OPTIONS} -fno-exceptions)

# ===== 依赖 =====
find_package(Threads REQUIRED)

include(GNUInstallDirs)

# ===== 日志库（自包含）=====
add_library(rslog STATIC
    src/log/log.cc
)
target_include_directories(rslog
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_compile_options(rslog PRIVATE ${RS_MODULE_COMPILE_OPTIONS})

# 会被链接进动态库，必须位置无关
set_target_properties(rslog PROPERTIES POSITION_INDEPENDENT_CODE ON)

# ===== 子模块 =====
add_subdirectory(src/rtp-packet)
add_subdirectory(src/rtsp-forward)

# ===== 应用 =====
if(BUILD_APP)
    add_executable(rtsp_server
        src/app/main.cc
    )
    target_link_libraries(rtsp_server PRIVATE
        rtp_packet
        rtsp_forward
        rslog
        Threads::Threads
    )
    target_compile_options(rtsp_server PRIVATE ${RS_MODULE_COMPILE_OPTIONS})

    # 输出到 build/bin，方便脚本调用
    set_target_properties(rtsp_server PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
endif()

# ===== 安装规则（可选）=====
if(ENABLE_INSTALL)
    install(TARGETS rslog
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
    install(FILES src/rtp-packet/include/rtp_packet.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
    install(FILES src/rtsp-forward/include/rtsp_forward.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
    if(BUILD_APP)
        install(TARGETS rtsp_server
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        )
    endif()
endif()

# ===== 构建摘要 =====
message(STATUS "")
message(STATUS "========== rtsp-server ${PROJECT_VERSION} ==========")
message(STATUS "构建类型  : ${CMAKE_BUILD_TYPE}")
message(STATUS "编译器    : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "动态库    : ${BUILD_SHARED_LIBS}")
message(STATUS "静态库    : ${BUILD_STATIC_LIBS}")
message(STATUS "应用      : ${BUILD_APP}")
message(STATUS "Werror    : ${ENABLE_WERROR}")
message(STATUS "=====================================================")
