#!/bin/bash
set -e

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
BUILD_DIR="${SCRIPT_DIR}/build"
CROSS_BUILD_DIR="${SCRIPT_DIR}/build-arm64"
TOOLCHAIN_FILE="${SCRIPT_DIR}/cmake/toolchain-aarch64-linux-gnu.cmake"

# The i.MX93 image's glibc. A library linked against anything newer will not
# load on the board.
BOARD_GLIBC="2.39"

usage() {
    echo "Usage: $0 <options>"
    echo ""
    echo "Options:"
    echo "  -b           Configure and build the library natively"
    echo "  -a           Cross-compile for the i.MX93 (aarch64), then verify the result"
    echo "  -c           Remove all build directories"
    echo "  -F           Format all sources with clang-format"
    echo "  -s <tool>    Run static analysis (tool: clang, cpp, both)"
    echo "  -t [filter]  Build and run the unit test suite."
    echo "                 With filter (GroupName or GroupName:TestName), run just that"
    echo "                 group/test instead of the full suite."
    echo "  -h           Print this help message"
    exit 0
}

# `cmake -S -B` re-runs configure every time, and configure re-runs CppUTest's
# CMakeLists, which prints a 37-line banner to stderr. Skip it when the tree is
# already configured: `cmake --build` reconfigures by itself if a CMakeLists
# changed, so nothing is lost.
ensure_configured() {
    if [ ! -f "${BUILD_DIR}/CMakeCache.txt" ]; then
        cmake -S "${SCRIPT_DIR}" -B "${BUILD_DIR}"
    fi
}

cmd_build() {
    cmake -S "${SCRIPT_DIR}" -B "${BUILD_DIR}"
    cmake --build "${BUILD_DIR}"
}

# A binary that builds is not a binary that runs on the board: check that it is
# actually aarch64, and that it asks for no glibc newer than the image ships.
verify_cross_build() {
    local library
    library=$(find "${CROSS_BUILD_DIR}" -name 'libmcuco.so.*.*' | head -1)

    if [ -z "${library}" ]; then
        echo "No shared library found in ${CROSS_BUILD_DIR}." >&2
        return 1
    fi

    echo ""
    echo "--- target check ---"
    file "${library}"

    if ! file "${library}" | grep -q "ARM aarch64"; then
        echo "Not an aarch64 binary." >&2
        return 1
    fi

    local floor
    floor=$(readelf -V "${library}" | grep -o 'GLIBC_[0-9.]*' | sort -Vu | tail -1)

    if [ -z "${floor}" ]; then
        echo "glibc floor: none (no versioned libc references yet)"
        return 0
    fi

    echo "glibc floor: ${floor}  (board has GLIBC_${BOARD_GLIBC})"

    local required="${floor#GLIBC_}"
    if [ "$(printf '%s\n%s\n' "${required}" "${BOARD_GLIBC}" | sort -V | tail -1)" != "${BOARD_GLIBC}" ]; then
        echo "Needs glibc ${required}, board has ${BOARD_GLIBC}." >&2
        return 1
    fi
}

cmd_cross_build() {
    cmake -S "${SCRIPT_DIR}" -B "${CROSS_BUILD_DIR}" --toolchain "${TOOLCHAIN_FILE}"
    cmake --build "${CROSS_BUILD_DIR}"
    verify_cross_build
}

cmd_format() {
    ensure_configured
    cmake --build "${BUILD_DIR}" --target format
}

# Analysis is off in a normal build: it roughly triples configure+build time and
# has nothing to say most of the time.
cmd_static_analysis() {
    local cert="OFF"
    local cppcheck="OFF"

    case "$1" in
        clang) cert="ON" ;;
        cpp)   cppcheck="ON" ;;
        both)  cert="ON"; cppcheck="ON" ;;
        *)
            echo "Unknown analysis tool: $1. Use clang, cpp, or both." >&2
            exit 1
            ;;
    esac

    local analysis_dir="${SCRIPT_DIR}/build-analysis"

    # clang-tidy and cppcheck run while compiling, so a warm tree compiles
    # nothing and analyses nothing while still reporting success. Start clean.
    rm -rf "${analysis_dir}"

    cmake -S "${SCRIPT_DIR}" -B "${analysis_dir}" \
        -DENABLE_CERT_CHECK="${cert}" -DENABLE_CPPCHECK="${cppcheck}"
    cmake --build "${analysis_dir}"
}

cmd_clean() {
    rm -rf "${BUILD_DIR}" "${CROSS_BUILD_DIR}" "${SCRIPT_DIR}/build-analysis"
    echo "Build directories removed."
}

cmd_test() {
    ensure_configured
    cmake --build "${BUILD_DIR}"
    ctest --test-dir "${BUILD_DIR}" --verbose
}

cmd_test_single() {
    local filter="$1"
    local group="${filter%%:*}"
    local name=""
    if [[ "${filter}" == *:* ]]; then
        name="${filter#*:}"
    fi

    ensure_configured
    cmake --build "${BUILD_DIR}"

    local cpputest_args=(-g "${group}")
    if [ -n "${name}" ]; then
        cpputest_args+=(-n "${name}")
    fi

    local output=""
    local result=0
    output=$("${BUILD_DIR}/library/unit_tests" "${cpputest_args[@]}" -v 2>&1) || result=$?

    printf '%s\n' "${output}"

    # CppUTest exits nonzero when a filter selects nothing, which reads as a
    # failure but only means the name was wrong.
    if [ "${result}" -ne 0 ] && [[ "${output}" == *"ran nothing"* ]]; then
        echo "No tests matched '${filter}'." >&2
        return 1
    fi

    return "${result}"
}

if [ $# -eq 0 ]; then
    usage
fi

while getopts "abcFs:th" opt; do
    case "${opt}" in
        a)
            cmd_cross_build
            ;;
        b)
            cmd_build
            ;;
        c)
            cmd_clean
            ;;
        F)
            cmd_format
            ;;
        s)
            cmd_static_analysis "${OPTARG}"
            ;;
        t)
            if [[ -n "${!OPTIND-}" && "${!OPTIND}" != -* ]]; then
                cmd_test_single "${!OPTIND}"
                OPTIND=$((OPTIND + 1))
            else
                cmd_test
            fi
            ;;
        h)
            usage
            ;;
        *)
            usage
            ;;
    esac
done
