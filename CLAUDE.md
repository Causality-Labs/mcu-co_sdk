# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

The **Linux host side** of mcu-co: a C shared library (`libmcuco.so`) that owns the
serial link to an STM32G474RE co-processor and exposes its UART command protocol as
typed C calls, plus a CLI built on top of it. C11, `gcc` natively and
`aarch64-linux-gnu-gcc` for the target board — the NXP i.MX93 FRDM (Cortex-A55,
aarch64) running Yocto `core-image-minimal` with glibc 2.39.

The firmware lives in a separate repo,
[Causality-Labs/mcu-co_firmware](https://github.com/Causality-Labs/mcu-co_firmware),
usually checked out alongside this one at `../mcu-co_firmware`. Two things there are
authoritative and must not be re-derived here:

- **`mcu-co_Protocol.md`** — the wire format. Frame layout, opcodes, field encodings,
  NACK reason codes, and worked example frames that double as test vectors.
- **`tools/mcu-co-cli/mcuco/`** — a working Python reference implementation. Open
  decision #3 in the protocol doc states the C SDK and this Python mock mirror each
  other.

## Build

Everything goes through `./buildmcu-co.sh` (`-h` for the full list): `-b` build
natively, `-t [filter]` build and run tests, `-a` cross-compile for the board, `-c`
remove build trees.

The toolchain file is passed explicitly rather than forced in `CMakeLists.txt` as the
firmware repo does: this library is host-runnable, and forcing the cross toolchain
would block the native test build. Tests sit behind `if(NOT CMAKE_CROSSCOMPILING)`.

`-a` verifies what a successful build does not: that the output is actually aarch64,
and that its glibc floor is **≤ 2.39**. Glibc is backward but not forward compatible,
so a higher floor builds fine and then refuses to load on the board.

Warnings are `-Werror` with the firmware's set, `PRIVATE` on the library target so a
consumer's build never fails on flags chosen here. New source files must be added to
`add_library(...)` — there is no glob.

## Architecture

The layers are visible in the file names; what isn't:

- **`uart.c` is not a UART driver.** The kernel owns the hardware (`cdc_acm` for
  `/dev/ttyACM0`, `ftdi_sio` for `/dev/ttyUSB0`, `imx-lpuart` on the board). It only
  asks the tty layer for the MCU's line settings.
- **Line settings must match `command_transport_init()` in the firmware**: 115200
  8N1, no flow control. `cfmakeraw()` is not optional — default tty settings strip
  the high bit of the `0xA5` SOF and rewrite CR/LF bytes that appear in CRCs.
- **`protocol.c` does zero I/O** — bytes in, bytes out, no file descriptors.

### Two rules that keep host and firmware from drifting

1. **Never encode the PWM pin-to-group map.** It comes from the STM32G4
   alternate-function table and lives only in the firmware's `peripherals/timer.c`.
   Address frequency by group number and duty by pin; let the MCU resolve the rest.
2. **Local validation covers ranges only** — port, pin, group, duty, frequency, enum
   membership. Policy (reserved pins, ownership, command ordering, EXTI-line
   conflicts) is the MCU's judgement and arrives as a NACK reason code. A second copy
   of those rules here is a second copy that can be wrong.

## Code style

- **Argument order is value-first** — `mcuco_gpio_set(mcu, level, port, pin)`, not
  `(mcu, port, pin, level)`. Matches CLI token order and payload byte order, so a
  call, the command that produced it and the bytes on the wire read the same
  direction. Open decision #3 in the protocol doc.
- **All static functions go at the top of a `.c`**, above every public one. Below
  them, public functions appear in the same order as their declarations in the `.h`.
- **Descriptive variable names.** No `t`, `n`, `rc`. Put the unit in the name where
  one exists (`deadline_ms`, `time_left_ms`).
- **Always parenthesize `sizeof`** — `sizeof(frame)`, not `sizeof frame`.
- **No `goto`.** If cleanup seems to need one, the function is doing two jobs — split
  it so each owns one resource, as `uart_open` / `configure_port` do.
- Error handling is return-code based: `0` or a positive count on success, a negative
  `errno` on failure.

## Comments — do not over-comment

Code should be self-documenting. Prefer a clear name or a restructured expression
over a comment explaining an unclear one. Do **not** narrate what the code plainly
says. If a comment restates the line below it, delete the comment.

Write a comment only when the code genuinely can't carry the meaning — a non-obvious
rationale, a constraint from the kernel or the protocol, or the reason an
obvious-looking alternative is wrong. E.g. `VMIN`/`VTIME` are zero in
`configure_port()` because VTIME is per-read and would multiply by the frame length;
the read-back after `tcsetattr()` exists because it reports success if it applied
*any* change, not all of them.

Public header declarations carry a short contract comment covering what the caller
can't infer — the return convention, ownership, what the function does *not*
guarantee. Not full Doxygen.

## Unit testing

Host-native **CppUTest** (`FetchContent`, pinned `v4.0`) in `library/tests/`. All test
files link into one `unit_tests` binary via `AllTests.cpp`.

- Test naming: `TEST(GroupName, BehaviorDescription)`, stating the guarantee rather
  than the implementation — `ReadDeadlineIsNotResetByAPartialRead`, not
  `TestReadLoop`. Group cases under `/* --- function_name --- */` per production
  function, with a one-line comment above each `TEST(...)` where the name isn't
  enough.
- Prioritize tests that guard the documented return/error contract plus one happy
  path, over exhaustive coverage of every edge case.
- **`openpty()` stands in for the board.** Open the slave through `uart_open` rather
  than using the raw fd — a default pty is canonical, and a binary frame with no
  newline never arrives. Needs `-lutil`.
- **Use real frames from `mcu-co_Protocol.md` as test data**, not invented bytes.

Building a new module test-first: invoke the `tdd` skill (`/tdd`).

## Scope discipline

Deliver the step that was asked for, not the step plus everything it implies. A
request to write one function is not licence to add install rules, a pkg-config file,
symbol-visibility control, or a second module alongside it. Those are separate steps
with their own approval.
