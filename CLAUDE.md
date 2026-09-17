# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

The **Linux host side** of mcu-co: a C shared library (`libmcuco.so`) that owns the
serial link to an STM32G474RE co-processor and exposes its UART command protocol as
typed C calls, plus a CLI built on top of it. C99, `gcc` natively and
`aarch64-linux-gnu-gcc` for the target board.

The firmware lives in a separate repo (`../mcu-co_firmware`). Two things there are
authoritative and must not be re-derived here:

- **`mcu-co_Protocol.md`** — the wire format. Frame layout, opcodes, field encodings,
  NACK reason codes, and worked example frames that double as test vectors.
- **`tools/mcu-co-cli/mcuco/`** — a working Python reference implementation
  (`protocol.py` codec, `link.py` transport, `client.py` API). Open decision #3 in the
  protocol doc states the C SDK and this Python mock are meant to mirror each other.

Target board is the NXP i.MX93 FRDM (Cortex-A55, aarch64) running a Yocto
`core-image-minimal` with glibc 2.39.

## Status

Only `library/src/uart.c` is implemented. `mcuco.c` and `protocol.c` are empty
placeholders; `cli/` and `examples/` are empty. There is no root `CMakeLists.txt` —
`library/` is currently a standalone CMake project.

## Layout

```
library/    the shared library - include/, src/, tests/
cli/        the mcu-co CLI (not started)
examples/   example programs (not started)
cmake/      toolchain-aarch64-linux-gnu.cmake
```

## Build

No wrapper script. CMake directly, from the repo root:

```bash
cmake -S library -B library/build          # native, builds the tests too
cmake --build library/build
ctest --test-dir library/build --verbose
```

Cross-compiling for the i.MX93 uses an explicit toolchain file — **not** a forced one
in `CMakeLists.txt`, unlike the firmware repo. The library is host-runnable, and
forcing the cross toolchain would block the native test build:

```bash
cmake -S library -B library/build-arm64 --toolchain cmake/toolchain-aarch64-linux-gnu.cmake
cmake --build library/build-arm64
```

The whole test block sits behind `if(NOT CMAKE_CROSSCOMPILING)`, so a cross build
produces only the library.

Compile options are `-Werror` with `-Wconversion -Wsign-conversion -Wshadow
-Wstrict-prototypes -Wmissing-prototypes` — the same set as the firmware. They are
`PRIVATE` on the library target: a consumer's build should not fail because of flags
chosen here.

When adding a source file, register it in `add_library(...)` in
`library/CMakeLists.txt` — there is no glob.

### Verifying a cross build

A binary that builds is not a binary that runs on the board. Check both:

```bash
file library/build-arm64/libmcuco.so.0.1.0                    # must say ARM aarch64
readelf -V library/build-arm64/libmcuco.so.0.1.0 | grep -o 'GLIBC_[0-9.]*' | sort -Vu | tail -1
```

The glibc floor must be **≤ 2.39**, the board image's version. Glibc is backward but
not forward compatible.

## Architecture

Three layers, mirroring the Python reference one-for-one, over a transport seam:

- **`protocol.c`** — CRC16-CCITT-FALSE, frame build and parse. **Zero I/O**: bytes in,
  bytes out, no file descriptors. Fully testable with no device.
- **`mcuco.c`** — the public API: one call per command, argument validation. Also
  owns the send-and-wait exchange (one command in flight, response timeout, resync
  to SOF) as a static function, since nothing else calls it.
- **`uart.c`** — POSIX termios. **Not a UART driver** — the kernel owns the hardware
  (`cdc_acm` for `/dev/ttyACM0`, `ftdi_sio` for `/dev/ttyUSB0`, `imx-lpuart` on the
  board). This only asks the tty layer for the MCU's line settings.

The library's line settings must match `command_transport_init()` in the firmware:
**115200 8N1, no flow control**. `cfmakeraw()` is not optional — default tty settings
strip the high bit of the `0xA5` SOF and rewrite CR/LF bytes that appear in CRCs.

### Two rules that keep host and firmware from drifting

1. **Never encode the PWM pin-to-group map.** It comes from the STM32G4
   alternate-function table and lives only in the firmware's `peripherals/timer.c`.
   Address frequency by group number and duty by pin; let the MCU resolve the rest.
2. **Local validation covers ranges only** — port, pin, group, duty, frequency, enum
   membership. Policy (reserved pins, ownership, command ordering, EXTI-line
   conflicts) is the MCU's judgement and arrives as a NACK reason code. A second copy
   of those rules here is a second copy that can be wrong.

## Code style

- **Argument order is value-first** — `mcuco_gpio_set(ctx, level, port, pin)`, not
  `(ctx, port, pin, level)`. This matches CLI token order and payload byte order, so
  a call, the command that produced it and the bytes on the wire read the same
  direction. Open decision #3 in the protocol doc; the Python mock follows it too.
- **All static functions go at the top of a `.c`**, above every public one. Below
  them, public functions appear in the same order as their declarations in the `.h`.
- **Descriptive variable names.** No `t`, `n`, `rc`, `buf`-adjacent single letters.
  Put the unit in the name where one exists (`deadline_ms`, `time_left_ms`).
- **Always parenthesize `sizeof`** — `sizeof(frame)`, not `sizeof frame`. Parens are
  only required for a type operand, but writing them always is one rule instead of
  two.
- **No `goto`.** If cleanup seems to need one, the function is doing two jobs — split
  it so each owns one resource, as `uart_open` / `configure_port` do.
- Error handling is return-code based: `0` or a positive count on success, a negative
  `errno` on failure. Validate arguments before use; avoid implicit conversions.

## Comments — do not over-comment

Code should be self-documenting. Prefer a clear name or a restructured expression
over a comment explaining an unclear one. Do **not** narrate what the code plainly
says (`/* increment the index */`, a comment above every field, a header block on
every trivial function).

Write a comment only when the code genuinely can't carry the meaning:

- **Why, not what** — a non-obvious rationale, a constraint from the kernel or the
  protocol, or the reason an obvious-looking alternative is wrong. E.g. `VMIN`/`VTIME`
  are zero in `configure_port()` because VTIME is per-read and would multiply by the
  frame length; the read-back after `tcsetattr()` exists because `tcsetattr` reports
  success if it applied *any* change, not all of them.
- **The one-line comment above each `TEST(...)`** required by the unit-test
  convention below.

Public header declarations carry a short contract comment covering what the caller
can't infer — the return convention, ownership, what the function does *not*
guarantee. Not full Doxygen.

If a comment restates the line below it, delete the comment.

## Unit testing

Host-native tests using **CppUTest** (fetched via CMake `FetchContent`, pinned to
`v4.0`) live in `library/tests/`.

- All test files link into one `unit_tests` binary via `AllTests.cpp`
  (`CommandLineTestRunner::RunAllTests`) — CppUTest convention: one runner, not one
  binary per module. `add_test()` bakes in `-v` so every `TEST(...)` name prints, not
  just failures.
- Test naming: `TEST(GroupName, BehaviorDescription)`, stating the guarantee rather
  than the implementation — `ReadDeadlineIsNotResetByAPartialRead`, not
  `TestReadLoop`. Group cases under a `/* --- function_name --- */` comment per
  production function, with a one-line comment above each `TEST(...)` where the intent
  isn't obvious from the name.
- Prioritize tests that guard the documented return/error contract plus one happy
  path, over exhaustive coverage of every edge case.
- **`openpty()` stands in for the board.** It gives both ends of a real tty, so
  `uart_open()` runs its actual code path against `/dev/pts/N`. Open the slave through
  `uart_open` rather than using the raw fd — a default pty is canonical, and a binary
  frame with no newline never arrives. Needs `-lutil`.
- **Use real frames from `mcu-co_Protocol.md` as test data**, not invented bytes. The
  doc's worked examples are valid frames and its "Test vectors" section lists the
  cases worth covering.

### TDD workflow for new modules

When building a new module test-first, follow *Test-Driven Development for Embedded
C*'s cycle:

1. **Write a test list before any test code.** A plain-language checklist of scenarios
   per function, saved as `library/tests/<module>_test_list.md`. Not a spec — cross
   items off, or add new ones, as work proceeds. Flag any open design question
   directly in the list rather than guessing; resolve it in discussion before writing
   tests against it.
2. **One test at a time, red then green.** Write exactly one failing test, build and
   confirm it fails for the expected reason (compile error, link error, or assertion
   failure — not just "some" error), then write the *minimal* implementation to pass
   that one test — not code that anticipates tests not yet written. Confirm green
   before moving on.
3. **Default to single-step pacing.** Stop after each individual step (write test /
   confirm red / write implementation / confirm green) and wait for confirmation
   rather than chaining the whole cycle unprompted in one pass — even if a prior
   message said "start on X," that authorizes one step, not the rest of the list. The
   user may explicitly grant broader autonomy ("make the rest of the module with this
   approach") to proceed through remaining list items without pausing; that grant
   doesn't carry over to the next module or session by default.
4. **Extending a fake is its own step.** If a test needs more from a fake than it
   currently exposes, add that capability first, confirm it doesn't break existing
   tests, *then* write the new test against it.
5. **Interface/protocol changes get discussed, not assumed.** If implementing a test
   reveals a real gap or ambiguity in the wire protocol or a locked-in interface, stop
   and raise it rather than silently extending the design — these are collaborative
   decisions.

## Scope discipline

Deliver the step that was asked for, not the step plus everything it implies. A
request to write one function is not licence to add install rules, a pkg-config file,
symbol-visibility control, or a second module alongside it. Those are separate steps
with their own approval.
