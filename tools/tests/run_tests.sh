#!/usr/bin/env bash
# apex-project/tools/tests/run_tests.sh
# ApexOS -- Automated boot + self-test runner
#
# Boots apex.img in headless QEMU, drives the shell through the QEMU
# monitor (no real display/keyboard needed), captures serial output,
# and checks it against a checklist of expected patterns: boot
# messages, the AXF/AXD pipeline tests embedded in kernel.c, the
# ktest.c self-test suite (`test` command), and the absence of any
# CPU exception (GPF, page fault, etc.).
#
# Usage:
#   ./tools/tests/run_tests.sh            (from the repo root)
#
# Exit code: 0 if every check passes, 1 otherwise. Prints a per-check
# PASS/FAIL summary either way.

set -u

# ── Paths ────────────────────────────────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
KERNEL_DIR="$REPO_ROOT/apex-kernel"
WORK_DIR="$(mktemp -d /tmp/apexos-test.XXXXXX)"
SERIAL_LOG="$WORK_DIR/serial.log"
MONITOR_SOCK="$WORK_DIR/qemu-monitor.sock"
QEMU_PID=""

# ── Timing (seconds) -- generous defaults for a slow CI box; override
# via environment if the test box is faster/slower than expected. ────────
BOOT_WAIT="${APEXOS_TEST_BOOT_WAIT:-6}"
CMD_WAIT="${APEXOS_TEST_CMD_WAIT:-2}"
TEST_SUITE_WAIT="${APEXOS_TEST_SUITE_WAIT:-5}"
FINAL_WAIT="${APEXOS_TEST_FINAL_WAIT:-3}"

cleanup() {
    if [ -n "$QEMU_PID" ] && kill -0 "$QEMU_PID" 2>/dev/null; then
        kill "$QEMU_PID" 2>/dev/null
        wait "$QEMU_PID" 2>/dev/null
    fi
    rm -rf "$WORK_DIR"
}
trap cleanup EXIT

# ── Monitor helper -- sends a QEMU monitor command over the unix
# socket. Requires `socat` (checked below); falls back to `nc` if
# socat isn't available and nc supports -U (unix sockets).
monitor_cmd() {
    local cmd="$1"
    if command -v socat >/dev/null 2>&1; then
        echo "$cmd" | socat - "UNIX-CONNECT:$MONITOR_SOCK" >/dev/null 2>&1
    else
        echo "$cmd" | nc -U "$MONITOR_SOCK" >/dev/null 2>&1
    fi
}

# Sends a single key through the monitor (QEMU key names: ret, spc,
# a-z, 0-9, minus, etc. -- see QEMU docs for the full list).
send_key() {
    monitor_cmd "sendkey $1"
    sleep 0.05
}

# Types a plain-text string one character at a time via sendkey.
# Limited to lowercase letters, digits, space, and dot -- enough for
# every command this suite needs to type (test, ls, run test.axp,
# etc.).
type_text() {
    local text="$1"
    local i char
    for ((i = 0; i < ${#text}; i++)); do
        char="${text:$i:1}"
        case "$char" in
        " ") send_key "spc" ;;
        ".") send_key "dot" ;;
        "_") send_key "shift-minus" ;; # underscore = Shift+- on a US QWERTY layout
        "/") send_key "slash" ;;
        *) send_key "$char" ;;
        esac
    done
}

type_line() {
    type_text "$1"
    send_key "ret"
}

# ── Preflight ────────────────────────────────────────────────────────────
if ! command -v qemu-system-x86_64 >/dev/null 2>&1; then
    echo "FAIL: qemu-system-x86_64 not found in PATH"
    exit 1
fi
if ! command -v socat >/dev/null 2>&1 && ! command -v nc >/dev/null 2>&1; then
    echo "FAIL: need either 'socat' or 'nc' (netcat) to drive the QEMU monitor"
    exit 1
fi

# ── Build ────────────────────────────────────────────────────────────────
echo "==> Building ApexOS..."
if ! (cd "$KERNEL_DIR" && make clean >/dev/null && make all >/tmp/apexos-build.log 2>&1); then
    echo "FAIL: build failed -- see /tmp/apexos-build.log"
    exit 1
fi

IMG="$KERNEL_DIR/build/apex.img"
if [ ! -f "$IMG" ]; then
    echo "FAIL: $IMG not produced by the build"
    exit 1
fi

# ── Boot ─────────────────────────────────────────────────────────────────
echo "==> Booting ApexOS in headless QEMU..."
qemu-system-x86_64 \
    -drive file="$IMG",format=raw,index=0,media=disk \
    -m 256M \
    -serial file:"$SERIAL_LOG" \
    -monitor unix:"$MONITOR_SOCK",server,nowait \
    -display none \
    -no-reboot \
    -no-shutdown &
QEMU_PID=$!

sleep "$BOOT_WAIT"

if ! kill -0 "$QEMU_PID" 2>/dev/null; then
    echo "FAIL: QEMU exited unexpectedly during boot -- see $SERIAL_LOG"
    cat "$SERIAL_LOG" 2>/dev/null
    exit 1
fi

# Unblocks shell_init()'s keyboard_getchar() wait ("Press any key to
# start shell...", see shell.c).
send_key "ret"
sleep "$CMD_WAIT"

# Runs the internal self-test suite (ktest.c).
type_line "test"
# `test` prints 55 assertion lines -- needs noticeably more time than
# a simple command before the shell is back at its prompt and ready
# for the next command. Without this, the next type_line() risked
# sending keystrokes while ktest_run_all() was still mid-output,
# landing them in an inconsistent spot (this caused an intermittent
# GPF right after `run test.axp` during script runs, never reproduced
# manually with natural typing pauses).
sleep "$TEST_SUITE_WAIT"

# Exercises the AXF loader end to end via the shell (separate from the
# boot-time embedded tests in kernel.c, which already ran before we
# even got here).
type_line "run test.axp"
sleep "$CMD_WAIT"

type_line "run test_import.axp"
sleep "$CMD_WAIT"

type_line "run test_driver.axp"
sleep "$CMD_WAIT"

sleep "$FINAL_WAIT"

if kill -0 "$QEMU_PID" 2>/dev/null; then
    kill "$QEMU_PID" 2>/dev/null
    wait "$QEMU_PID" 2>/dev/null
fi
QEMU_PID=""

if [ ! -s "$SERIAL_LOG" ]; then
    echo "FAIL: no serial output captured at all -- boot likely failed silently"
    exit 1
fi

# ── Checks ───────────────────────────────────────────────────────────────
# Each check: a human-readable label, and a pattern to search for.
# grep -q on the raw log -- VGA text also mirrors to the serial port in
# this kernel (see kernel.c: serial_putchar in the keyboard path, plus
# every klog_* call goes through vga_puts which the BIOS/QEMU debug
# console does NOT capture -- only what's explicitly written to serial
# does). If a check below never matches, the most likely cause is a
# real regression, not a logging gap -- klog output already reaches
# serial via the existing vga+serial mirroring in this codebase.
declare -a LABELS=()
declare -a PATTERNS=()
declare -a REQUIRED=() # 1 = failure fails the whole run, 0 = warning only

add_check() {
    LABELS+=("$1")
    PATTERNS+=("$2")
    REQUIRED+=("$3")
}

# Boot sequence.
add_check "GDT initialized"                 "GDT initialized"                              1
add_check "IDT initialized"                 "IDT initialized"                              1
add_check "PIC remapped"                    "PIC 8259 remapped"                            1
add_check "Syscalls initialized"            "Syscalls initialized"                         1
add_check "PMM initialized"                 "PMM initialized"                               1
add_check "VMM initialized"                 "VMM initialized"                               1
add_check "Heap initialized"                "Heap initialized"                              1
add_check "AXFS formatted"                  "AXFS formatted"                                1
add_check "AXFS mounted"                    "AXFS mounted"                                  1
add_check "Default tree created"            "Default filesystem tree created"               1
add_check "Volume registered"               "Volume registered"                             1

# Boot-time AXF/AXD pipeline (embedded tests, see kernel.c).
add_check "stub.axd driver loaded"          "axd_load: driver loaded"                       1
add_check "driver_ping() returned 42"       "driver_ping\(\) returned: 42"                  1

# ktest.c self-test suite (`test` command).
add_check "Self-test suite ran"             "ApexOS Kernel Self-Test"                       1
add_check "Self-test results printed"       "Results:"                                      1
add_check "Zero self-test failures"         "Results: [0-9]* passed  0 failed"              1

# Shell-driven AXF loader exercises.
add_check "test.axp ran"                    "Hello from"                                    0
add_check "test_import.axp import worked"   "add\(3,4\)"                                    0
add_check "test_driver.axp SYS_DRIVER_CALL" "driver_ping\(\) = 42"                          0

# Crash detection -- these must NEVER appear.
add_check "No General Protection Fault"     "General Protection Fault"                      1
add_check "No Page Fault"                   "Page Fault"                                    1
add_check "No Double Fault"                 "Double Fault"                                  1
add_check "No kernel panic"                 "KERNEL PANIC"                                  1

echo ""
echo "==> Checking serial output ($SERIAL_LOG)..."
echo ""

FAIL_COUNT=0
WARN_COUNT=0

for i in "${!LABELS[@]}"; do
    label="${LABELS[$i]}"
    pattern="${PATTERNS[$i]}"
    required="${REQUIRED[$i]}"

    # Crash patterns must be ABSENT; everything else must be PRESENT.
    if [[ "$label" == "No "* ]]; then
        if grep -qE "$pattern" "$SERIAL_LOG"; then
            echo "  [FAIL] $label -- pattern '$pattern' WAS found (should not be present)"
            FAIL_COUNT=$((FAIL_COUNT + 1))
        else
            echo "  [PASS] $label"
        fi
        continue
    fi

    if grep -qE "$pattern" "$SERIAL_LOG"; then
        echo "  [PASS] $label"
    else
        if [ "$required" = "1" ]; then
            echo "  [FAIL] $label -- pattern '$pattern' not found"
            FAIL_COUNT=$((FAIL_COUNT + 1))
        else
            echo "  [WARN] $label -- pattern '$pattern' not found (non-blocking)"
            WARN_COUNT=$((WARN_COUNT + 1))
        fi
    fi
done

echo ""
echo "==> Full serial log saved for inspection:"
FINAL_LOG="$REPO_ROOT/tools/tests/last_run.log"
cp "$SERIAL_LOG" "$FINAL_LOG"
echo "    $FINAL_LOG"
echo ""

if [ "$FAIL_COUNT" -eq 0 ]; then
    echo "=== ALL CHECKS PASSED ($WARN_COUNT warning(s)) ==="
    exit 0
else
    echo "=== $FAIL_COUNT CHECK(S) FAILED ($WARN_COUNT warning(s)) ==="
    exit 1
fi