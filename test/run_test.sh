#!/usr/bin/env bash
# ============================================================
# 端到端测试：rtsp_server + ffmpeg
#
# 覆盖：
#   1. 用 ffmpeg 生成 H.264 测试码流
#   2. 启动服务器，验证端口监听
#   3. ffprobe 校验 RTSP 流的编码/分辨率/帧率
#   4. ffmpeg 拉流 5 秒并完整解码，校验帧数与解码错误
#   5. 两路客户端并发拉流，校验服务端会话统计
#   6. 优雅退出（SIGINT）
#
# 用法: ./test/run_test.sh [端口，默认 8554]
# 依赖: ffmpeg / ffprobe / cmake 已构建好的 build/bin/rtsp_server
# ============================================================
set -uo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="${ROOT_DIR}/build/bin/rtsp_server"
DATA_DIR="${ROOT_DIR}/test/data"
OUT_DIR="${ROOT_DIR}/test/output"
PORT="${1:-8554}"
RTSP_URL="rtsp://127.0.0.1:${PORT}/stream"
SERVER_LOG="${OUT_DIR}/server.log"

PASS=0
FAIL=0
SERVER_PID=""

# ---------- 工具函数 ----------
info()  { printf '\033[36m[INFO]\033[0m %s\n' "$*"; }
pass()  { PASS=$((PASS + 1)); printf '\033[32m[PASS]\033[0m %s\n' "$*"; }
fail()  { FAIL=$((FAIL + 1)); printf '\033[31m[FAIL]\033[0m %s\n' "$*"; }
die()   { printf '\033[31m[FATAL]\033[0m %s\n' "$*"; cleanup; exit 1; }

cleanup() {
    if [[ -n "${SERVER_PID}" ]] && kill -0 "${SERVER_PID}" 2>/dev/null; then
        kill -INT "${SERVER_PID}" 2>/dev/null || true
        for _ in $(seq 1 30); do
            kill -0 "${SERVER_PID}" 2>/dev/null || break
            sleep 0.1
        done
        kill -9 "${SERVER_PID}" 2>/dev/null || true
    fi
}
trap cleanup EXIT

# ---------- 前置检查 ----------
for tool in ffmpeg ffprobe; do
    command -v "${tool}" >/dev/null 2>&1 || die "缺少依赖: ${tool}"
done

[[ -x "${BIN}" ]] || die "未找到可执行文件 ${BIN}，请先运行 ./scripts/build.sh"

mkdir -p "${DATA_DIR}" "${OUT_DIR}"
H264_FILE="${DATA_DIR}/test.h264"
rm -f "${OUT_DIR}"/*.mp4 "${SERVER_LOG}"

# ---------- 1. 生成测试码流 ----------
info "生成 H.264 测试码流（10 秒 / 640x480 / 25fps / GOP 25）"
ffmpeg -hide_banner -loglevel error -y \
    -f lavfi -i "testsrc2=duration=10:size=640x480:rate=25" \
    -c:v libx264 -profile:v baseline -level 3.0 -g 25 -keyint_min 25 -bf 0 \
    -x264-params "no-scenecut=1" \
    -f h264 "${H264_FILE}" || die "ffmpeg 生成码流失败"

SIZE=$(stat -c%s "${H264_FILE}" 2>/dev/null || echo 0)
if [[ "${SIZE}" -gt 1000 ]]; then
    pass "测试码流已生成 (${SIZE} 字节)"
else
    die "测试码流异常 (${SIZE} 字节)"
fi

# ---------- 2. 启动服务器 ----------
info "启动 rtsp_server（端口 ${PORT}）"
"${BIN}" -f "${H264_FILE}" -p "${PORT}" --fps 25 --log-level debug \
    >"${SERVER_LOG}" 2>&1 &
SERVER_PID=$!

READY=0
for _ in $(seq 1 50); do
    if ! kill -0 "${SERVER_PID}" 2>/dev/null; then
        echo "--- 服务端日志 ---"; cat "${SERVER_LOG}"; die "服务器进程提前退出"
    fi
    if (exec 3<>"/dev/tcp/127.0.0.1/${PORT}") 2>/dev/null; then
        exec 3<&- 3>&-
        READY=1
        break
    fi
    sleep 0.2
done
[[ "${READY}" -eq 1 ]] && pass "端口 ${PORT} 已监听" || die "端口 ${PORT} 未就绪"

if grep -q "解析完成:" "${SERVER_LOG}"; then
    pass "码流解析: $(grep -o '解析完成:.*' "${SERVER_LOG}" | head -1)"
else
    fail "未在日志中找到码流解析结果"
fi

# ---------- 3. ffprobe 校验流信息 ----------
info "ffprobe 校验 RTSP 流"
PROBE_JSON="${OUT_DIR}/probe.json"
ffprobe -hide_banner -loglevel error -rtsp_transport tcp \
    -show_streams -of json "${RTSP_URL}" >"${PROBE_JSON}" 2>"${OUT_DIR}/probe.err"

CODEC=$(grep -o '"codec_name": *"[^"]*"' "${PROBE_JSON}" | head -1 | sed 's/.*: *"//;s/"//')
WIDTH=$(grep -o '"width": *[0-9]*' "${PROBE_JSON}" | head -1 | grep -o '[0-9]*')
HEIGHT=$(grep -o '"height": *[0-9]*' "${PROBE_JSON}" | head -1 | grep -o '[0-9]*')

[[ "${CODEC}" == "h264" ]] && pass "编码格式正确: ${CODEC}" \
    || fail "编码格式异常: '${CODEC}'（期望 h264）"
[[ "${WIDTH}" == "640" && "${HEIGHT}" == "480" ]] && pass "分辨率正确: ${WIDTH}x${HEIGHT}" \
    || fail "分辨率异常: ${WIDTH}x${HEIGHT}（期望 640x480）"

# ---------- 4. 拉流并完整解码 ----------
info "ffmpeg 拉流 5 秒（TCP）"
PULL_FILE="${OUT_DIR}/pull.mp4"
ffmpeg -hide_banner -loglevel error -y -rtsp_transport tcp \
    -i "${RTSP_URL}" -t 5 -c copy -an "${PULL_FILE}" 2>"${OUT_DIR}/pull.err"

PULL_SIZE=0
[[ -f "${PULL_FILE}" ]] && PULL_SIZE=$(stat -c%s "${PULL_FILE}" 2>/dev/null || echo 0)
if [[ "${PULL_SIZE}" -gt 10000 ]]; then
    pass "拉流成功 (${PULL_SIZE} 字节)"
else
    cat "${OUT_DIR}/pull.err" 2>/dev/null | head -5
    fail "拉流文件过小 (${PULL_SIZE} 字节)"
fi

FRAMES=$(ffprobe -hide_banner -loglevel error -count_frames -select_streams v:0 \
    -show_entries stream=nb_read_frames -of csv=p=0 "${PULL_FILE}" 2>/dev/null)
FRAMES=${FRAMES:-0}
# 5 秒 @25fps 期望 ~125 帧，留 20% 容差
if [[ "${FRAMES}" -ge 100 ]]; then
    pass "解码帧数正常: ${FRAMES} 帧（5 秒 @25fps）"
else
    fail "解码帧数不足: ${FRAMES} 帧（期望 >=100）"
fi

# 完整解码一遍，任何解码错误都会写 stderr
DECODE_ERR=$(ffmpeg -hide_banner -v error -i "${PULL_FILE}" -f null - 2>&1 | head -5)
if [[ -z "${DECODE_ERR}" ]]; then
    pass "整段解码无错误"
else
    fail "解码报错: ${DECODE_ERR}"
fi

# 抽一帧确认画面非全黑（testsrc2 有彩条与噪点）
ffmpeg -hide_banner -loglevel error -y -i "${PULL_FILE}" -frames:v 1 \
    -vf "scale=160:120" "${OUT_DIR}/frame.png" 2>/dev/null
if [[ -s "${OUT_DIR}/frame.png" ]]; then
    pass "成功抽取关键帧: test/output/frame.png"
else
    fail "抽帧失败"
fi

# ---------- 5. 双客户端并发 ----------
info "两路客户端并发拉流"
ffmpeg -hide_banner -loglevel error -y -rtsp_transport tcp -i "${RTSP_URL}" \
    -t 3 -c copy -an "${OUT_DIR}/c1.mp4" &
C1=$!
ffmpeg -hide_banner -loglevel error -y -rtsp_transport tcp -i "${RTSP_URL}" \
    -t 3 -c copy -an "${OUT_DIR}/c2.mp4" &
C2=$!
wait ${C1}; R1=$?
wait ${C2}; R2=$?

C1_SIZE=$([[ -f "${OUT_DIR}/c1.mp4" ]] && stat -c%s "${OUT_DIR}/c1.mp4" || echo 0)
C2_SIZE=$([[ -f "${OUT_DIR}/c2.mp4" ]] && stat -c%s "${OUT_DIR}/c2.mp4" || echo 0)
if [[ ${R1} -eq 0 && ${R2} -eq 0 && "${C1_SIZE}" -gt 5000 && "${C2_SIZE}" -gt 5000 ]]; then
    pass "双客户端并发正常 (${C1_SIZE} / ${C2_SIZE} 字节)"
else
    fail "并发拉流异常 (rc=${R1}/${R2}, size=${C1_SIZE}/${C2_SIZE})"
fi

if grep -q "累计连接" "${SERVER_LOG}"; then
    CONN=$(grep -o "累计连接=[0-9]*" "${SERVER_LOG}" | tail -1)
    pass "服务端会话统计: ${CONN}"
else
    fail "服务端未输出会话统计"
fi

# ---------- 6. 优雅退出 ----------
info "发送 SIGINT，检查优雅退出"
kill -INT "${SERVER_PID}" 2>/dev/null
EXITED=0
for _ in $(seq 1 50); do
    if ! kill -0 "${SERVER_PID}" 2>/dev/null; then
        EXITED=1
        break
    fi
    sleep 0.1
done
wait "${SERVER_PID}" 2>/dev/null
SERVER_PID=""

[[ "${EXITED}" -eq 1 ]] && pass "收到 SIGINT 后正常退出" || fail "进程未在 5 秒内退出"

if grep -q "服务器已停止" "${SERVER_LOG}"; then
    pass "退出日志完整"
else
    fail "缺少退出日志"
fi

# 检查是否有崩溃/内存错误痕迹
if grep -qi "segmentation\|core dumped\|AddressSanitizer" "${SERVER_LOG}"; then
    fail "服务端日志中出现崩溃痕迹"
else
    pass "无崩溃痕迹"
fi

# ---------- 汇总 ----------
echo
echo "=========================================="
printf '通过: \033[32m%d\033[0m  失败: \033[31m%d\033[0m\n' "${PASS}" "${FAIL}"
echo "产物目录: ${OUT_DIR}"
echo "服务端日志: ${SERVER_LOG}"
echo "=========================================="

[[ "${FAIL}" -eq 0 ]] && exit 0 || exit 1
