#!/bin/bash
# =============================================================================
# PyOCD GDB Server 启动脚本（健壮版）
# 用途：检测环境 → 构建项目 → 启动 PyOCD GDB Server，供 VSCode/GDB 连接调试
# 使用：./start_debug.sh
# =============================================================================

# -----------------------------------------------------------------------------
# Bash 严格模式
# -----------------------------------------------------------------------------
# set -e           : 任何命令返回非 0（失败）就立即退出脚本，不会"带病前进"
# set -u           : 引用未定义的变量直接报错退出（防止拼错变量名导致诡异 bug）
# set -o pipefail  : 管道中任意一段失败，整个管道视为失败
#                    （默认 bash 只看管道最后一段的退出码，会漏掉中间的错误）
# 这三个选项加上之后，脚本出错会立刻停下来，不会偷偷继续执行
set -euo pipefail


# -----------------------------------------------------------------------------
# 路径定位（不依赖任何用户名或绝对路径）
# -----------------------------------------------------------------------------
# ${BASH_SOURCE[0]} 是当前脚本文件的路径（比 $0 更可靠，处理 source 调用也对）
# dirname 取目录部分，cd 进去再 pwd 得到绝对路径
# 这样无论从哪里调用脚本，SCRIPT_DIR 都是脚本文件所在目录的绝对路径
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# 项目根目录：脚本在 VscodeGcc/scripts/ 下，往上两级就是项目根
# ../.. 表示上两级目录，cd + pwd 把相对路径转成绝对路径
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"


# -----------------------------------------------------------------------------
# 配置参数（可根据项目调整）
# -----------------------------------------------------------------------------
TARGET="fm33le01x"                                              # PyOCD 的目标芯片名
FREQUENCY="1000000"                                             # SWD 时钟频率 (Hz)
PACK_FILE="$PROJECT_DIR/VscodeGcc/FM33LE0XX_DFP.1.0.2.pack"     # CMSIS-Pack 文件
PORT="3333"                                                     # GDB Server 端口
TELNET_PORT="4444"                                              # PyOCD telnet 控制端口
LOG_FILE="$SCRIPT_DIR/pyocd_server.log"                         # PyOCD 输出日志

# PyOCD 可执行文件路径
# ${VAR:-default} 语法：VAR 未定义或为空时使用 default 值
# 这样允许用户从命令行临时覆盖：
#   PYOCD_BIN=/some/path/pyocd ./start_debug.sh
# 留空则触发后面的 find_pyocd 自动探测（推荐）
PYOCD_BIN="${PYOCD_BIN:-}"


# -----------------------------------------------------------------------------
# 颜色定义（让输出更清晰，没有颜色支持的终端会自动忽略）
# -----------------------------------------------------------------------------
# \033[ 是 ANSI 转义序列前缀，后面跟数字代码控制颜色/样式
# 31m=红 32m=绿 33m=黄 36m=青 0m=重置（必须重置否则后面文字都是这个颜色）
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
CYAN='\033[0;36m'
NC='\033[0m'  # No Color，重置颜色

# 封装几个打印函数，统一风格、便于全局修改
# echo -e 启用转义序列解析（不加 -e 的话 \033 会被原样打印）
log_info()    { echo -e "${CYAN}[INFO]${NC}  $*"; }
log_ok()      { echo -e "${GREEN}[ OK ]${NC}  $*"; }
log_warn()    { echo -e "${YELLOW}[WARN]${NC}  $*"; }
log_error()   { echo -e "${RED}[FAIL]${NC}  $*" >&2; }   # 错误打到 stderr
log_section() { echo -e "\n${CYAN}=== $* ===${NC}"; }


# -----------------------------------------------------------------------------
# 端口监听检测（多方案降级）
# -----------------------------------------------------------------------------
# 不同 Linux 发行版自带的网络工具不一样：
#   - 新系统（Debian 12+/Ubuntu 22.04+）默认有 ss，没 netstat
#   - 老系统多数有 netstat（来自 net-tools 包）
#   - macOS 默认有 lsof
#   - 最后兜底直接读 /proc/net/tcp（Linux 内核接口，几乎一定存在）
# 探测顺序：ss → netstat → lsof → /proc/net/tcp
# 命中即用，全部失败才报错
check_port_listening() {
    local port="$1"

    if command -v ss >/dev/null 2>&1; then
        # ss -lnt：l=listening, n=numeric(不解析名字), t=tcp
        # grep -q：quiet，不输出，只用退出码表达"找到/没找到"
        ss -lnt 2>/dev/null | grep -q ":${port} "
    elif command -v netstat >/dev/null 2>&1; then
        netstat -lnt 2>/dev/null | grep -q ":${port} "
    elif command -v lsof >/dev/null 2>&1; then
        # lsof -iTCP:端口 -sTCP:LISTEN：找正在监听这个 TCP 端口的进程
        lsof -iTCP:"$port" -sTCP:LISTEN >/dev/null 2>&1
    elif [ -r /proc/net/tcp ]; then
        # 最后兜底：/proc/net/tcp 里端口是十六进制大写
        # printf '%04X' 把十进制端口转成 4 位大写十六进制（3333 → 0D05）
        local hex_port
        hex_port=$(printf '%04X' "$port")
        # 第二列格式是 "本地IP:端口"，第四列是连接状态，0A 表示 LISTEN
        grep -q ":${hex_port} 0A" /proc/net/tcp
    else
        # 真的找不到任何方法，假设端口空闲（让后续 PID 检查兜底）
        return 1
    fi
}


# -----------------------------------------------------------------------------
# 自动探测 pyocd 位置（支持 venv / pipx / 全局安装）
# -----------------------------------------------------------------------------
# 设计目标：让脚本"开箱即用"，不要求用户手动改路径
# 探测顺序（按用户意图强度排序）：
#   1. 用户已通过 PYOCD_BIN 显式指定 → 最高优先级
#   2. 当前 shell 已激活 venv（$VIRTUAL_ENV 非空）→ 用 venv 里的
#   3. 项目目录里的 venv → 项目专用环境
#   4. 用户家目录常见 venv 位置 → 个人统一管理
#   5. pipx 安装位置（~/.local/bin）
#   6. PATH 里的 pyocd → 全局安装
# 找到第一个能用的就停，没找到返回非 0
find_pyocd() {
    # ---- 1. 用户显式指定，最高优先级 ----
    # -n 测试字符串非空（与 -z 相反）
    # -x 测试文件存在且可执行
    if [ -n "$PYOCD_BIN" ]; then
        if [ -x "$PYOCD_BIN" ]; then
            echo "$PYOCD_BIN"
            return 0
        else
            log_error "PYOCD_BIN 指定的路径不可执行: $PYOCD_BIN"
            return 1
        fi
    fi

    # ---- 2. 当前已激活的 venv ----
    # $VIRTUAL_ENV 是 venv activate 时设置的环境变量，指向 venv 根目录
    # ${VAR:-} 防止 set -u 在变量未定义时报错（替代为空字符串）
    if [ -n "${VIRTUAL_ENV:-}" ] && [ -x "$VIRTUAL_ENV/bin/pyocd" ]; then
        echo "$VIRTUAL_ENV/bin/pyocd"
        return 0
    fi

    # ---- 3. 项目目录里的 venv（常见命名）----
    # 用 for 循环挨个检查几种常见的 venv 目录名
    local venv_dir
    for venv_dir in "$PROJECT_DIR/venv" "$PROJECT_DIR/.venv" "$PROJECT_DIR/env"; do
        if [ -x "$venv_dir/bin/pyocd" ]; then
            echo "$venv_dir/bin/pyocd"
            return 0
        fi
    done

    # ---- 4. 用户家目录常见位置 ----
    # $HOME 是当前用户家目录，bash 自动设置，永远正确
    # 这里列几个我知道大家常用的 venv 摆放位置
    local home_path
    for home_path in \
        "$HOME/venv/pyocd/bin/pyocd" \
        "$HOME/.venv/pyocd/bin/pyocd" \
        "$HOME/venvs/pyocd/bin/pyocd" \
        "$HOME/Program/pyocd/bin/pyocd" \
        "$HOME/Program/pyocd/venv/bin/pyocd" \
        "$HOME/Program/PyOCD/venv_new/bin/pyocd" \
        "$HOME/Program/PyOCD/venv/bin/pyocd" \
        "$HOME/Program/pyOCD/venv/bin/pyocd"
    do
        if [ -x "$home_path" ]; then
            echo "$home_path"
            return 0
        fi
    done

    # ---- 5. pipx 安装位置 ----
    # pipx 把每个工具装在 ~/.local/pipx/venvs/<name>/，并在 ~/.local/bin/ 留软链
    if [ -x "$HOME/.local/bin/pyocd" ]; then
        echo "$HOME/.local/bin/pyocd"
        return 0
    fi

    # ---- 6. PATH 里的 pyocd（全局安装）----
    # command -v 输出命令的实际路径
    if command -v pyocd >/dev/null 2>&1; then
        command -v pyocd
        return 0
    fi

    return 1
}


# -----------------------------------------------------------------------------
# 依赖检测：缺什么提示装什么
# -----------------------------------------------------------------------------
# 在干任何事之前先把"必备工具"检查一遍，缺啥列清单一次性告知
# 比"跑到一半才发现缺工具"用户体验好得多
check_dependencies() {
    log_section "检查依赖"

    # 用数组收集所有缺失项，最后一次性打印
    # bash 数组：missing=()  追加：missing+=("...")  长度：${#missing[@]}
    local missing=()

    # --- pyocd：必须有，自动探测 ---
    # 调用 find_pyocd，把输出（路径）赋给全局变量 PYOCD_BIN
    # 后续启动时直接用 $PYOCD_BIN
    local found_pyocd
    if found_pyocd=$(find_pyocd); then
        PYOCD_BIN="$found_pyocd"
        log_ok "pyocd: $PYOCD_BIN"

        # 顺便打印来源，方便用户确认用的是哪个环境
        case "$PYOCD_BIN" in
            "${VIRTUAL_ENV:-/__none__}"/*)  log_info "  来源: 当前激活的 venv" ;;
            "$PROJECT_DIR"/*)               log_info "  来源: 项目目录 venv" ;;
            "$HOME/.local/bin/"*)           log_info "  来源: pipx 或用户级 pip" ;;
            "$HOME"/*)                      log_info "  来源: 用户家目录 venv" ;;
            *)                              log_info "  来源: 系统 PATH" ;;
        esac
    else
        missing+=("pyocd        | 安装方式（任选其一）：")
        missing+=("             |   pipx install pyocd                          (推荐，独立 venv)")
        missing+=("             |   python3 -m venv ~/venv/pyocd && \\")
        missing+=("             |     ~/venv/pyocd/bin/pip install pyocd        (手动 venv)")
        missing+=("             |   pip install --user pyocd                    (用户级)")
        missing+=("             | 或设置环境变量指定已有路径：")
        missing+=("             |   export PYOCD_BIN=/path/to/your/pyocd")
    fi

    # --- cmake：构建用 ---
    if ! command -v cmake >/dev/null 2>&1; then
        missing+=("cmake        | sudo apt install cmake")
    else
        log_ok "cmake: $(command -v cmake)"
    fi

    # --- arm-none-eabi-gcc：交叉编译器 ---
    if ! command -v arm-none-eabi-gcc >/dev/null 2>&1; then
        missing+=("arm-none-eabi-gcc | sudo apt install gcc-arm-none-eabi")
    else
        log_ok "arm-gcc: $(command -v arm-none-eabi-gcc)"
    fi

    # --- 端口检测工具（任一即可）---
    if   command -v ss      >/dev/null 2>&1; then log_ok "端口检测: ss"
    elif command -v netstat >/dev/null 2>&1; then log_ok "端口检测: netstat"
    elif command -v lsof    >/dev/null 2>&1; then log_ok "端口检测: lsof"
    elif [ -r /proc/net/tcp ];                then log_ok "端口检测: /proc/net/tcp"
    else
        missing+=("ss/netstat   | sudo apt install iproute2  或  sudo apt install net-tools")
    fi

    # 有缺失就报错退出，并列出每项的安装方法
    # ${#missing[@]} 是数组长度，-gt 0 表示大于 0
    if [ ${#missing[@]} -gt 0 ]; then
        log_error "缺少以下依赖（左侧是命令名，右侧是建议的安装方式）："
        # printf '  %s\n' 配合数组展开，每行打印一项
        printf '         %s\n' "${missing[@]}"
        exit 1
    fi
}


# -----------------------------------------------------------------------------
# 文件/路径检查：确认 pack 文件、项目目录这些必备资源都在
# -----------------------------------------------------------------------------
check_files() {
    log_section "检查项目文件"

    log_info "脚本目录: $SCRIPT_DIR"
    log_info "项目目录: $PROJECT_DIR"

    # -f 测试是不是普通文件（存在且不是目录）
    if [ ! -f "$PACK_FILE" ]; then
        log_error "Pack 文件不存在: $PACK_FILE"
        log_error "请确认 .pack 文件路径，或修改脚本顶部的 PACK_FILE 变量"
        exit 1
    fi
    log_ok "Pack 文件: $PACK_FILE"

    # -d 测试是不是目录
    if [ ! -d "$PROJECT_DIR" ]; then
        log_error "项目目录不存在: $PROJECT_DIR"
        exit 1
    fi
}


# -----------------------------------------------------------------------------
# 优雅停止已存在的 PyOCD 进程
# -----------------------------------------------------------------------------
# 启动新 server 之前，要先确保没有老 server 在抢端口
# 策略：先 SIGTERM 让进程自己清理退出，给 3 秒；不行再 SIGKILL 强杀
stop_existing_pyocd() {
    # pgrep -f：在完整命令行里搜（不只是进程名），匹配 "pyocd...gdbserver"
    # >/dev/null 把输出丢掉，只关心退出码
    if ! pgrep -f "pyocd.*gdbserver" >/dev/null; then
        return 0  # 没有现存进程，直接返回成功
    fi

    log_warn "检测到已有 PyOCD GDB Server 在运行"

    # 如果端口还在监听，问用户是否要重启
    if check_port_listening "$PORT"; then
        log_info "现有服务器正在监听端口 $PORT"
        # -p 提示词，-r 不解析反斜杠（防止用户输入被吞），结果存入 restart 变量
        read -r -p "是否要重启? [y/N]: " restart
        # =~ 是正则匹配，^[Yy]$ 表示只接受单个 y 或 Y
        if [[ ! "$restart" =~ ^[Yy]$ ]]; then
            log_info "保留现有服务器，退出"
            exit 0
        fi
    fi

    log_info "正在停止现有 PyOCD..."
    # SIGTERM (-TERM 或 -15)：优雅停止，进程能捕获并清理资源
    pkill -TERM -f "pyocd.*gdbserver" || true   # || true 防止 set -e 在没杀到时退出
    sleep 3

    # 还在的话强杀
    if pgrep -f "pyocd.*gdbserver" >/dev/null; then
        log_warn "优雅停止超时，使用 SIGKILL 强制结束"
        # SIGKILL (-KILL 或 -9)：内核直接杀，进程没机会清理
        pkill -KILL -f "pyocd.*gdbserver" || true
        sleep 1
    fi

    if pgrep -f "pyocd.*gdbserver" >/dev/null; then
        log_error "无法停止现有 PyOCD 进程，请手动处理"
        exit 1
    fi
    log_ok "现有 PyOCD 已停止"
}


# -----------------------------------------------------------------------------
# 构建项目
# -----------------------------------------------------------------------------
build_project() {
    log_section "构建项目"

    # 切到项目根目录执行 cmake 命令
    # 注意：因为 set -e 开了，cmake 失败脚本会自动退出，不需要手动 if [ $? -ne 0 ]
    cd "$PROJECT_DIR"

    # 没有 build 目录就先 configure
    if [ ! -d "build" ]; then
        log_info "build 目录不存在，正在执行 cmake 配置..."
        mkdir -p build
        # -S 指定源码根，-B 指定构建目录，是 cmake 现代用法
        # 比传统的 "cd build && cmake .." 更稳，不用切目录
        cmake -S . -B build
    fi

    log_info "开始编译..."
    cmake --build build
    log_ok "编译完成"
}


# -----------------------------------------------------------------------------
# 启动 PyOCD GDB Server
# -----------------------------------------------------------------------------
start_pyocd() {
    log_section "启动 PyOCD GDB Server"

    log_info "目标芯片: $TARGET"
    log_info "端口    : $PORT (GDB) / $TELNET_PORT (telnet)"
    log_info "日志    : $LOG_FILE"

    cd "$PROJECT_DIR"

    # nohup：忽略 SIGHUP 信号，让进程在脚本退出后继续运行
    # 末尾的 & ：放到后台
    # > "$LOG_FILE" 2>&1 ：标准输出和标准错误都重定向到日志文件
    #   2>&1 必须放在 > "$LOG_FILE" 之后，顺序错了不生效
    # \  在行尾表示续行，让长命令分多行写更可读
    # 注意用 "$PYOCD_BIN" 而不是 pyocd，因为前面 find_pyocd 可能定位到 venv 里
    nohup "$PYOCD_BIN" gdbserver \
        --target "$TARGET" \
        --frequency "$FREQUENCY" \
        --pack "$PACK_FILE" \
        --port "$PORT" \
        --telnet-port "$TELNET_PORT" \
        --persist \
        > "$LOG_FILE" 2>&1 &

    # $! 是上一个后台进程的 PID
    local pyocd_pid=$!
    log_ok "PyOCD 已启动 (PID: $pyocd_pid)"

    # ---- 关键改进：立刻验证进程没崩 ----
    # 给 pyocd 1 秒启动时间，然后检查 PID 是否还存在
    # kill -0 不真的发信号，只检查"能不能给这个 PID 发信号"
    # 进程存在且有权限 → 退出码 0；进程已死 → 非 0
    sleep 1
    if ! kill -0 "$pyocd_pid" 2>/dev/null; then
        log_error "PyOCD 启动后立即退出！日志内容："
        echo "----- $LOG_FILE -----"
        # sed 给每行加缩进，方便阅读
        sed 's/^/    /' "$LOG_FILE"
        echo "----- end -----"
        exit 1
    fi

    # ---- 等待端口监听起来 ----
    # 进程活着不代表服务好了，还要等端口真的 listen 上
    # 最多等 15 秒（每秒检查一次）
    log_info "等待端口 $PORT 监听..."
    local max_wait=15
    local i
    for ((i = 1; i <= max_wait; i++)); do
        # 每次循环都再检查一次进程是否还活着，防止启动过程中崩溃
        if ! kill -0 "$pyocd_pid" 2>/dev/null; then
            log_error "PyOCD 在启动过程中退出！日志内容："
            sed 's/^/    /' "$LOG_FILE"
            exit 1
        fi

        if check_port_listening "$PORT"; then
            log_ok "PyOCD GDB Server 监听中 (端口 $PORT)"
            print_success_message
            return 0
        fi
        sleep 1
    done

    # 循环结束还没监听 → 超时
    log_error "等待 ${max_wait}s 后仍未监听端口 $PORT，可能启动失败"
    log_error "请查看日志: $LOG_FILE"
    exit 1
}


# -----------------------------------------------------------------------------
# 启动成功后的提示信息
# -----------------------------------------------------------------------------
print_success_message() {
    echo
    log_ok "调试环境就绪！"
    echo
    echo "  下一步可以："
    echo "    • 在 VSCode 中按 F5 开始调试"
    echo "    • 或命令行启动 GDB:"
    echo "        arm-none-eabi-gdb $PROJECT_DIR/build/BT_PassThroughFT.elf"
    echo "        (gdb) target remote localhost:$PORT"
    echo
    echo "  管理命令："
    echo "    • 查看日志:   tail -f $LOG_FILE"
    echo "    • 停止服务:   $SCRIPT_DIR/stop_debug.sh"
    echo
}


# =============================================================================
# 主流程：按顺序执行各阶段
# =============================================================================
# 把 main 单独写成函数，让脚本结构更清晰，也方便以后调用
main() {
    echo "=========================================="
    echo "  BT_NR_TRANSLATOR (fm33le01x) Debug Helper"
    echo "=========================================="

    check_dependencies
    check_files
    stop_existing_pyocd
    build_project
    start_pyocd
}

# "$@" 把脚本接收到的所有参数原样传给 main
# （现在 main 没用参数，但留这个习惯，以后扩展 --no-build 之类选项很方便）
main "$@"
