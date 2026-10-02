#!/bin/bash
# Runs every mcu-co-cli command against a connected MCU and checks the results.
#
# usage: smoke_test.sh [device]        default device: /dev/ttyACM0
#        MCUCO_CLI=path/to/mcu-co-cli smoke_test.sh   to test a build that is not installed
#
# Needs no wiring beyond the MCU itself: nothing here depends on pressing a button.

DEVICE="${1:-/dev/ttyACM0}"
CLI="${MCUCO_CLI:-mcu-co-cli}"

# An output pin that has a PWM channel on PWM_TIMER, an input pin, and a pin
# with no PWM channel at all. Change these to suit the board.
OUT_PORT=A
OUT_PIN=5
IN_PORT=C
IN_PIN=13
PWM_TIMER=0
NO_PWM_PORT=A
NO_PWM_PIN=0

passed=0
failed=0

run_cli() {
    "${CLI}" -d "${DEVICE}" "$@" 2>/dev/null
}

report() {
    local result="$1"
    local description="$2"

    if [ "${result}" = "pass" ]; then
        passed=$((passed + 1))
        printf '  PASS  %s\n' "${description}"
    else
        failed=$((failed + 1))
        printf '  FAIL  %s\n' "${description}"
    fi
}

expect_ok() {
    local description="$1"
    shift

    if run_cli "$@" >/dev/null; then
        report pass "${description}"
    else
        report fail "${description}  (${CLI} $*)"
    fi
}

expect_fail() {
    local description="$1"
    shift

    if run_cli "$@" >/dev/null; then
        report fail "${description}  (${CLI} $* succeeded)"
    else
        report pass "${description}"
    fi
}

expect_output() {
    local description="$1"
    local expected="$2"
    shift 2

    local output
    output=$(run_cli "$@")

    if [ "${output}" = "${expected}" ]; then
        report pass "${description}"
    else
        report fail "${description}  (expected '${expected}', got '${output}')"
    fi
}

# A reset drops the link while the MCU reboots, so keep probing until it answers.
reset_mcu() {
    run_cli mcu reset >/dev/null

    for _ in {1..10}; do
        sleep 0.5
        if run_cli mcu probe >/dev/null; then
            return 0
        fi
    done

    echo "MCU did not come back after reset." >&2
    exit 1
}

if ! command -v "${CLI}" >/dev/null; then
    echo "${CLI} not found. Install it, or set MCUCO_CLI to its path." >&2
    exit 1
fi

echo "Testing ${CLI} on ${DEVICE}"

if ! run_cli mcu probe >/dev/null; then
    echo "No MCU answering on ${DEVICE}. Check the device and the dialout group." >&2
    exit 1
fi

echo "mcu"
expect_ok     "probe"                                   mcu probe
reset_mcu
report pass   "reset, then the MCU answers again"

echo "gpio"
expect_ok     "configure ${OUT_PORT}${OUT_PIN} as output"  gpio cfg output "${OUT_PORT}" "${OUT_PIN}"
expect_ok     "set ${OUT_PORT}${OUT_PIN} high"             gpio set high "${OUT_PORT}" "${OUT_PIN}"
expect_ok     "set ${OUT_PORT}${OUT_PIN} low"              gpio set low "${OUT_PORT}" "${OUT_PIN}"
expect_output "toggle from low reports high"               high gpio toggle "${OUT_PORT}" "${OUT_PIN}"
expect_output "toggle from high reports low"               low  gpio toggle "${OUT_PORT}" "${OUT_PIN}"
expect_ok     "configure ${IN_PORT}${IN_PIN} as input"     gpio cfg input "${IN_PORT}" "${IN_PIN}"
level=$(run_cli gpio get "${IN_PORT}" "${IN_PIN}")
if [ "${level}" = "high" ] || [ "${level}" = "low" ]; then
    report pass "read ${IN_PORT}${IN_PIN} (${level})"
else
    report fail "read ${IN_PORT}${IN_PIN}  (got '${level}')"
fi

echo "timer and pwm"
reset_mcu
expect_ok     "configure timer ${PWM_TIMER} at 1 kHz"      timer cfg 1000 "${PWM_TIMER}"
expect_output "timer ${PWM_TIMER} achieved 1000 Hz"        1000 timer get "${PWM_TIMER}"
expect_fail   "reconfigure a running timer is refused"     timer cfg 2000 "${PWM_TIMER}"
expect_ok     "claim ${OUT_PORT}${OUT_PIN} for pwm"        pwm cfg active-high "${OUT_PORT}" "${OUT_PIN}"
expect_output "a claimed pin starts at 0.0%"               0.0 pwm get "${OUT_PORT}" "${OUT_PIN}"
expect_ok     "set duty to 25%"                            pwm set 25 "${OUT_PORT}" "${OUT_PIN}"
expect_output "duty reads back 25.0%"                      25.0 pwm get "${OUT_PORT}" "${OUT_PIN}"
expect_ok     "set duty to 100%"                           pwm set 100 "${OUT_PORT}" "${OUT_PIN}"
expect_output "duty reads back 100.0%"                     100.0 pwm get "${OUT_PORT}" "${OUT_PIN}"
expect_ok     "release ${OUT_PORT}${OUT_PIN}"              pwm release "${OUT_PORT}" "${OUT_PIN}"
expect_ok     "release timer ${PWM_TIMER}"                 timer release "${PWM_TIMER}"

echo "irq"
reset_mcu
expect_ok     "configure ${IN_PORT}${IN_PIN} as input"     gpio cfg input "${IN_PORT}" "${IN_PIN}"
expect_ok     "configure ${OUT_PORT}${OUT_PIN} as output"  gpio cfg output "${OUT_PORT}" "${OUT_PIN}"
expect_ok     "arm a falling-edge trigger"                 irq cfg falling "${IN_PORT}" "${IN_PIN}"
expect_ok     "bind it to toggle ${OUT_PORT}${OUT_PIN}"    irq bind falling "${IN_PORT}" "${IN_PIN}" toggle "${OUT_PORT}" "${OUT_PIN}"
expect_fail   "binding twice is refused"                   irq bind falling "${IN_PORT}" "${IN_PIN}" toggle "${OUT_PORT}" "${OUT_PIN}"
expect_ok     "unbind"                                     irq unbind "${IN_PORT}" "${IN_PIN}"
expect_ok     "disarm"                                     irq cfg off "${IN_PORT}" "${IN_PIN}"

echo "errors"
reset_mcu
expect_fail   "port out of range is rejected"              gpio set high H 5
expect_fail   "pin out of range is rejected"               gpio set high A 16
expect_fail   "a pin with no pwm channel is refused"       pwm cfg active-high "${NO_PWM_PORT}" "${NO_PWM_PIN}"
expect_fail   "an unknown command is rejected"             gpio blink A 5
if "${CLI}" -d /dev/nonexistent mcu probe >/dev/null 2>&1; then
    report fail "a missing device is reported"
else
    report pass "a missing device is reported"
fi

reset_mcu

echo ""
echo "${passed} passed, ${failed} failed"

if [ "${failed}" -ne 0 ]; then
    exit 1
fi
