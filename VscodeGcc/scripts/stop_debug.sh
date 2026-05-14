#!/bin/bash
# =============================================================================
# PyOCD GDB Server 停止脚本（健壮版）
# 用途：优雅停止所有 PyOCD GDB Server 进程，可选清理日志
# 使用：./stop_debug.sh           交互式停止
#       ./stop_debug.sh -y        非交互模式（不问任何问题，直接停止+清日志）
#       ./stop_debug.sh --keep    停止但保留日志（不询问）
# =============================================================================

# 严格模式（说明见 start_debug.sh 的注释）
set -euo pipefail


# -----------------------------------------------------------------------------
# 路径定位
# -----------------------------------------------------------------------------
# 跟 start_debug.sh 的逻辑一样：定位脚本自身目录，不依赖任何用户名
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LOG_FILE="$SCRIPT_DIR/pyocd_server.log"


# -----------------------------------------------------------------------------
# 颜色和日志函数（跟 start 脚本保持一致的风格）
# -----------------------------------------------------------------------------
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
CYAN='\033[0;36m'
NC='\033[0m'

log_info()    { echo -e "${CYAN}[INFO]${NC}  $*"; }
log_ok()      { echo -e "${GREEN}[ OK ]${NC}  $*"; }
log_warn()    { echo -e "${YELLOW}[WARN]${NC}  $*"; }
log_error()   { echo -e "${RED}[FAIL]${NC}  $*" >&2; }
log_section() { echo -e "\n${CYAN}=== $* ===${NC}"; }


# -----------------------------------------------------------------------------
# 参数解析
# -----------------------------------------------------------------------------
# 这里用最简单的方式：直接 case 匹配几个固定选项
# 复杂场景应该用 getopts/getopt，但本脚本只有 2-3 个选项就够用
AUTO_YES=false       # -y：清理日志不询问
KEEP_LOG=false       # --keep：保留日志不询问

# while 循环处理所有参数，shift 把 $1 移走，下次循环看新的 $1
# $# 是剩余参数个数，> 0 就继续
while [ $# -gt 0 ]; do
    case "$1" in
        -y|--yes)
            AUTO_YES=true
            shift
            ;;
        --keep|--keep-log)
            KEEP_LOG=true
            shift
            ;;
        -h|--help)
            # cat << EOF 是 Here Document，把 EOF 之间的内容原样输出
            # 适合打印多行帮助文本
            cat << EOF
用法: $(basename "$0") [选项]

选项:
  -y, --yes        非交互模式，自动清理日志
  --keep           保留日志文件，不询问
  -h, --help       显示此帮助信息

示例:
  $(basename "$0")          交互式停止
  $(basename "$0") -y       停止并自动清理日志
  $(basename "$0") --keep   停止但保留日志
EOF
            exit 0
            ;;
        *)
            log_error "未知参数: $1"
            log_error "使用 -h 查看帮助"
            exit 1
            ;;
    esac
done


# -----------------------------------------------------------------------------
# 停止 PyOCD：先 SIGTERM 优雅停止，超时再 SIGKILL
# -----------------------------------------------------------------------------
stop_pyocd() {
    log_section "停止 PyOCD GDB Server"

    # ---- 检查有没有进程要停 ----
    # pgrep -f "pyocd.*gdbserver"：在完整命令行里搜匹配的进程
    # 输出所有匹配的 PID（每行一个）
    # 用 || true 防止 pgrep 没找到时返回非 0 触发 set -e 退出
    local pids
    pids=$(pgrep -f "pyocd.*gdbserver" || true)

    # -z 测试字符串是否为空（zero length）
    if [ -z "$pids" ]; then
        log_info "没有运行中的 PyOCD GDB Server，无需停止"
        return 0
    fi

    # 把多行 PID 转成一行展示，看着舒服
    # tr '\n' ' ' 把换行替换成空格
    log_info "找到 PyOCD 进程: $(echo "$pids" | tr '\n' ' ')"

    # ---- 第一阶段：SIGTERM 优雅停止 ----
    # SIGTERM (信号 15)：通知进程"请停止"
    # 进程可以捕获这个信号，做一些清理工作（关文件、断开连接）再退出
    # 这是停止后台服务的标准做法，对硬件资源（USB 调试器）尤其重要
    log_info "发送 SIGTERM (优雅停止)..."
    pkill -TERM -f "pyocd.*gdbserver" || true

    # ---- 等待最多 5 秒看进程是否自己退出 ----
    # 用循环+短间隔比单次 sleep 5 好：
    # 1. 进程秒退的话不用傻等满 5 秒
    # 2. 能精确知道实际花了多久
    local max_wait=5
    local waited=0
    while [ $waited -lt $max_wait ]; do
        # pgrep 返回 0 表示找到了，非 0 表示没找到（已经停了）
        if ! pgrep -f "pyocd.*gdbserver" >/dev/null; then
            log_ok "PyOCD 已优雅停止（耗时 ${waited}s）"
            return 0
        fi
        sleep 1
        # bash 整数运算用 $((...))，比 expr 快也比 let 现代
        waited=$((waited + 1))
    done

    # ---- 第二阶段：SIGKILL 强制杀 ----
    # SIGTERM 不奏效（5 秒还没退）就上 SIGKILL
    # SIGKILL (信号 9)：内核强制终止，进程没机会清理
    # 副作用：可能留下未释放的 USB 句柄等，下次启动可能要重插 USB
    log_warn "进程未在 ${max_wait}s 内停止，使用 SIGKILL 强制结束"
    pkill -KILL -f "pyocd.*gdbserver" || true
    sleep 1

    # 最后确认一下，强杀都不死那系统就有问题了
    if pgrep -f "pyocd.*gdbserver" >/dev/null; then
        log_error "强制杀进程失败，请手动检查："
        log_error "  ps -ef | grep pyocd"
        exit 1
    fi
    log_ok "PyOCD 已强制停止"
}


# -----------------------------------------------------------------------------
# 检查并报告端口释放情况
# -----------------------------------------------------------------------------
# 进程停了不一定立刻释放端口（TCP TIME_WAIT 状态会保留一段时间）
# 这里只是给个提示，不强制要求端口立刻空闲
check_port_released() {
    local port=3333

    # 跟 start_debug.sh 同样的多方案降级
    local in_use=false
    if command -v ss >/dev/null 2>&1; then
        ss -lnt 2>/dev/null | grep -q ":${port} " && in_use=true
    elif command -v netstat >/dev/null 2>&1; then
        netstat -lnt 2>/dev/null | grep -q ":${port} " && in_use=true
    fi

    if $in_use; then
        log_warn "端口 $port 仍在使用中（可能是 TIME_WAIT 状态，稍后会自动释放）"
    fi
}


# -----------------------------------------------------------------------------
# 处理日志文件
# -----------------------------------------------------------------------------
handle_log_file() {
    # 日志不存在直接跳过
    if [ ! -f "$LOG_FILE" ]; then
        return 0
    fi

    # du -h：human-readable 格式（K/M/G）
    # cut -f1：只取第一列（大小，不要后面的文件名）
    local size
    size=$(du -h "$LOG_FILE" 2>/dev/null | cut -f1)
    log_info "日志文件: $LOG_FILE (大小: $size)"

    # ---- 判断要不要删 ----
    # --keep 优先：明确要保留就直接返回
    if $KEEP_LOG; then
        log_info "保留日志文件 (--keep)"
        return 0
    fi

    # -y 优先：明确要删就直接删
    if $AUTO_YES; then
        rm -f "$LOG_FILE"
        log_ok "日志文件已清理 (-y)"
        return 0
    fi

    # 默认：交互式询问
    # -t 0 测试 stdin 是不是连接终端：是终端才询问，被 pipe 调用就跳过
    # 这样 ./stop_debug.sh </dev/null 这种自动化场景不会卡住等输入
    if [ -t 0 ]; then
        read -r -p "是否清理日志文件? [y/N]: " clean_log
        if [[ "$clean_log" =~ ^[Yy]$ ]]; then
            rm -f "$LOG_FILE"
            log_ok "日志文件已清理"
        else
            log_info "保留日志文件"
        fi
    else
        # 非交互场景默认保留，避免误删
        log_info "非交互模式，保留日志文件（用 -y 强制清理）"
    fi
}


# =============================================================================
# 主流程
# =============================================================================
main() {
    echo "=========================================="
    echo "  PyOCD GDB Server 停止脚本"
    echo "=========================================="

    stop_pyocd
    check_port_released
    handle_log_file

    echo
    log_ok "完成"
}

main "$@"
