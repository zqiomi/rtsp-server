# ============================================================
# rtp_packet（H.264/H.265 → RTP 封包）
#
# 作为子模块被顶层引入，构建选项由顶层统一控制。
# ============================================================

project(rtp_packet
    VERSION 1.0.0
    DESCRIPTION "轻量级RTP打包库，支持H.264/H.265"
    LANGUAGES CXX)

# ===== 源文件列表 =====
set(RTP_PACKET_SOURCES
    src/rtp/rtp_header.cc
    src/rtcp/rtcp.cc
    src/codec/nalu_splitter.cc
    src/codec/h264_packer.cc
    src/codec/h265_packer.cc
    src/codec/packer_factory.cc
    src/api/rtp_packet.cc
)

# ===== 公共编译属性 =====
function(rtp_packet_configure_target target)
    target_include_directories(${target}
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/include
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_compile_definitions(${target} PRIVATE
        RTP_PACKET_VERSION_STRING="${PROJECT_VERSION}"
        RS_LOG_MODULE_NS=rtp_packet
        RS_LOG_MODULE_NAME="rtp_packet"
    )
    target_compile_options(${target} PRIVATE ${RS_MODULE_COMPILE_OPTIONS})
    target_link_libraries(${target} PUBLIC rslog Threads::Threads)
endfunction()

# ===== 动态库 =====
if(BUILD_SHARED_LIBS)
    add_library(rtp_packet_shared SHARED ${RTP_PACKET_SOURCES})
    set_target_properties(rtp_packet_shared PROPERTIES
        OUTPUT_NAME rtp_packet
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
    )
    rtp_packet_configure_target(rtp_packet_shared)
endif()

# ===== 静态库 =====
if(BUILD_STATIC_LIBS)
    add_library(rtp_packet_static STATIC ${RTP_PACKET_SOURCES})
    set_target_properties(rtp_packet_static PROPERTIES
        OUTPUT_NAME rtp_packet
        POSITION_INDEPENDENT_CODE ON
    )
    rtp_packet_configure_target(rtp_packet_static)
endif()

# ===== 供上层链接的统一别名 =====
if(BUILD_SHARED_LIBS)
    add_library(rtp_packet ALIAS rtp_packet_shared)
else()
    add_library(rtp_packet ALIAS rtp_packet_static)
endif()
