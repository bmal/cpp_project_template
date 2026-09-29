#!/usr/bin/env bash
# Prepares a Linux machine for repeatable benchmark numbers and restores it when you press Enter.
# Sets the performance governor, turns turbo off, and prints the taskset command that pins a run.
set -euo pipefail

usage() {
    cat <<USAGE
Usage: scripts/bench-env.sh [--cpu <n>]

Sets every CPU to the performance governor and turns turbo off, through sudo when not root.
Prints the command that pins a benchmark to CPU <n>, default the last one.
Restores the previous settings on Enter, Ctrl-C, or any exit.
USAGE
}

cpu=""
while [ "$#" -gt 0 ]; do
    case "$1" in
    --cpu)
        cpu="${2:?--cpu needs a CPU number}"
        shift 2
        ;;
    -h | --help)
        usage
        exit 0
        ;;
    *)
        usage >&2
        exit 2
        ;;
    esac
done

if [ "$(uname -s)" != Linux ]; then
    echo "bench-env: Linux only; on macOS, quit other applications and stay on power" >&2
    exit 1
fi
if [ -z "${cpu}" ]; then cpu="$(($(nproc) - 1))"; fi

sudo=""
if [ "$(id -u)" -ne 0 ]; then sudo="sudo"; fi

# One "path<TAB>previous value" line per setting changed, restored in reverse order.
saved=()
restore() {
    local i path value
    for ((i = ${#saved[@]} - 1; i >= 0; i--)); do
        path="${saved[i]%%$'\t'*}"
        value="${saved[i]#*$'\t'}"
        echo "${value}" | ${sudo} tee "${path}" >/dev/null ||
            echo "bench-env: could not restore ${path}" >&2
    done
    if [ "${#saved[@]}" -gt 0 ]; then echo "bench-env: restored ${#saved[@]} settings"; fi
}
trap restore EXIT
trap 'exit 130' INT TERM

# Writes $2 to the sysfs file $1 if it exists and differs, remembering the old value.
set_value() {
    local path="$1" value="$2" old
    [ -e "${path}" ] || return 0
    old="$(cat "${path}")"
    [ "${old}" != "${value}" ] || return 0
    echo "${value}" | ${sudo} tee "${path}" >/dev/null
    saved+=("${path}"$'\t'"${old}")
}

for governor in /sys/devices/system/cpu/cpu[0-9]*/cpufreq/scaling_governor; do
    set_value "${governor}" performance
done
set_value /sys/devices/system/cpu/intel_pstate/no_turbo 1
set_value /sys/devices/system/cpu/cpufreq/boost 0

if [ ! -e /sys/devices/system/cpu/cpu0/cpufreq ]; then
    echo "bench-env: no CPU frequency controls, as in a virtual machine; numbers stay noisy"
fi
echo "bench-env: pin a benchmark to CPU ${cpu} from another terminal:"
echo "  taskset -c ${cpu} build/bench/bin/parser_benchmarks --benchmark_repetitions=10"
read -r -p "bench-env: press Enter to restore the machine. " _ || true
