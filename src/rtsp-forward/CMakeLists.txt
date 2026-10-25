# ============================================================
# rtsp_forward（RTSP 信令 + RTP 多路转发，epoll 实现）
#
# 作为子模块被顶层引入，构建选项由顶层统一控制。
# 依赖：rslog（日志）、Threads、epoll（Linux）
# ============================================================

project(rtsp_forward
    VERSION 1.2.0
    DESCRIPTION "轻量级RTSP转发服务器库"
    LANGUAGES CXX)

# ===== 源文件列表 =====
set(RTSP_FORWARD_SOURCES
    src/buffer/ring_buffer.cc
    src/net/epoll_loop.cc
    src/net/socket.cc
    src/net/connection.cc
    src/net/listener.cc
    src/protocol/rtsp_parser.cc
    src/protocol/rtsp_builder.cc
    src/rtp/rtp_forwarder.cc
    src/core/rtsp_session.cc
    src/core/rtsp_server.cc
    src/api/rtsp_forward.cc
)

# ===== 公共编译属性 =====
function(rtsp_forward_configure_target target)
    target_include_directories(${target}
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/include
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_compile_definitions(${target} PRIVATE
        RTSP_FORWARD_VERSION_STRING="${PROJECT_VERSION}"
        RS_LOG_MODULE_NS=rtsp_forward
        RS_LOG_MODULE_NAME="rtsp_forward"
    )
    target_compile_options(${target} PRIVATE ${RS_MODULE_COMPILE_OPTIONS})
    target_link_libraries(${target} PUBLIC rslog Threads::Threads)
endfunction()

# ===== 动态库 =====
if(BUILD_SHARED_LIBS)
    add_library(rtsp_forward_shared SHARED ${RTSP_FORWARD_SOURCES})
    set_target_properties(rtsp_forward_shared PROPERTIES
        OUTPUT_NAME rtsp_forward
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
    )
    rtsp_forward_configure_target(rtsp_forward_shared)
endif()

# ===== 静态库 =====
if(BUILD_STATIC_LIBS)
    add_library(rtsp_forward_static STATIC ${RTSP_FORWARD_SOURCES})
    set_target_properties(rtsp_forward_static PROPERTIES
        OUTPUT_NAME rtsp_forward
        POSITION_INDEPENDENT_CODE ON
    )
    rtsp_forward_configure_target(rtsp_forward_static)
endif()

# ===== 供上层链接的统一别名 =====
if(BUILD_SHARED_LIBS)
    add_library(rtsp_forward ALIAS rtsp_forward_shared)
else()
    add_library(rtsp_forward ALIAS rtsp_forward_static)
endif()
