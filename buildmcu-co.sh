#!/bin/bash
set -e

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
LIBRARY_DIR="${SCRIPT_DIR}/library"
BUILD_DIR="${LIBRARY_DIR}/build"
CROSS_BUILD_DIR="${LIBRARY_DIR}/build-arm64"
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
    echo "  -t [filter]  Build and run the unit test suite."
    echo "                 With filter (GroupName or GroupName:TestName), run just that"
    echo "                 group/test instead of the full suite."
    echo "  -h           Print this help message"
    exit 0
}

cmd_build() {
    cmake -S "${LIBRARY_DIR}" -B "${BUILD_DIR}"
    cmake --build "${BUILD_DIR}"
}

# A binary that builds is not a binary that runs on the board: check that it is
# actually aarch64, and that it asks for no glibc newer than the image ships.
verify_cross_build() {
    local library
    library=$(find "${CROSS_BUILD_DIR}" -maxdepth 1 -name 'libmcuco.so.*.*' | head -1)

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
    cmake -S "${LIBRARY_DIR}" -B "${CROSS_BUILD_DIR}" --toolchain "${TOOLCHAIN_FILE}"
    cmake --build "${CROSS_BUILD_DIR}"
    verify_cross_build
}

cmd_clean() {
    rm -rf "${BUILD_DIR}" "${CROSS_BUILD_DIR}"
    echo "Build directories removed."
}

cmd_test() {
    cmake -S "${LIBRARY_DIR}" -B "${BUILD_DIR}"
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

    cmake -S "${LIBRARY_DIR}" -B "${BUILD_DIR}"
    cmake --build "${BUILD_DIR}"

    local cpputest_args=(-g "${group}")
    if [ -n "${name}" ]; then
        cpputest_args+=(-n "${name}")
    fi

    local output=""
    local result=0
    output=$("${BUILD_DIR}/unit_tests" "${cpputest_args[@]}" -v 2>&1) || result=$?

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

while getopts "abcth" opt; do
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
