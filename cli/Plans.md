# mcu-co CLI — command-line design

The planned surface for `mcu-co-cli`, the CLI built on `libmcuco.so`. This is the
grammar and its rules; `mcu-co_Protocol.md` in the firmware repo remains
authoritative for the wire format and is not restated here.

Status: shape agreed, nothing implemented. Two picks are still open, marked
**OPEN** below.

## Shape

```
mcu-co-cli -s <subsystem> -c <command> [ flags ]
```

Nothing is positional. Every argument is a flag, so parsing is a single
`getopt_long` pass, fully order-independent — `-s gpio -c set` and `-c set -s gpio`
are the same command — and the parser expects zero non-option arguments, which
turns a dropped flag into an error instead of a silently ignored token.

### Universal rules

- Argument flags always take a value, spelled in full. `-s gpio`, never `-s g`.
- An unknown value errors, listing the valid set.
- A repeated flag is an error, not getopt's silent last-wins. `-s gpio -s pwm` is
  a typo or a bad shell expansion, never an intentional override.
- A flag not valid for the `(subsystem, command)` pair is rejected by name:
  `-s gpio -c get -l high` → *"gpio get takes no --level"*.
- `-q`, `-h` and `-V` are the only booleans. They are true on/off switches with
  nothing to be exclusive against; everywhere else a value-taking flag is what
  makes "exactly one of" structural rather than a runtime check.

## The 16 commands

One row per command in the protocol reference.

```
link   -c probe
       -c reset

gpio   -c cfg     -p <pin> -d <input|output>
       -c set     -p <pin> -l <low|high>
       -c get     -p <pin>                       → low | high
       -c toggle  -p <pin>                       → low | high   (after the flip)

irq    -c cfg     -p <pin> -e <off|rising|falling|both>
       -c bind    -p <pin> -e <rising|falling|both> -a <low|high|toggle> --to <pin>
       -c unbind  -p <pin>

timer  -c cfg     -t <0-2> -f <1-1000000>
       -c get     -t <0-2>                       → achieved Hz
       -c release -t <0-2>

pwm    -c cfg     -p <pin> [--polarity <active-high|active-low>]
       -c set     -p <pin> -u <duty>
       -c get     -p <pin>                       → duty
       -c release -p <pin>
```

A `(subsystem, command)` pair outside this table is rejected, naming the commands
that subsystem does take.

## Flags

| flag | long | values | notes |
|---|---|---|---|
| `-s` | `--subsystem` | `gpio` `irq` `timer` `pwm` `link` | |
| `-c` | `--command` | depends on `-s` | |
| `-p` | `--pin` | `A5` — port A–G, pin 0–15 | optional leading `P`, case-insensitive |
| `-d` | `--direction` | `input` `output` | |
| `-l` | `--level` | `low` `high` | |
| `-e` | `--edge` | `off` `rising` `falling` `both` | `off` refused by `bind` |
| `-a` | `--action` | `low` `high` `toggle` | |
| `-t` | `--timer` | `0` `1` `2` | **OPEN**: `-t` vs `-n/--number` |
| `-f` | `--freq` | `1`–`1000000` Hz | plain integer |
| `-u` | `--duty` | `0`–`100`, one decimal | **OPEN**: percent vs raw tenths |
| | `--to` | second pin | `irq bind` only |
| | `--polarity` | `active-high` (default), `active-low` | `pwm cfg` only |

Long-only where a flag is typed rarely: `--to`, `--polarity`, and the globals.
`-v` is left free for a future `--verbose`.

### `-p` accepts

```
A5   a5   PA5   pa5   A05
```

Rejected, each naming which half is wrong: `H0` (port must be A–G), `A16` (pin
must be 0–15), `5A` (expected `<port><pin>`), `A` (missing pin number).

There is no batch form — the protocol is one command per pin.

## Per-subsystem notes

Things a user will otherwise get wrong. These belong in the help text.

### gpio

`toggle` prints the level it ended up at, taken from the response DATA. It is
free, and it saves a `get` round trip.

### irq

- **The edge is typed twice on `bind` by design.** The firmware requires the bound
  edge to match the armed edge exactly, because EXTI cannot report which edge
  fired. It is a confirmation, not a second setting; a mismatch is
  `ERR_INVALID_STATE`.
- **`unbind` and `cfg -e off` are different teardowns.** `unbind` drops the action
  and leaves the trigger armed. `cfg -e off` disarms the pin *and* clears its
  bindings.
- **One output per input.** The firmware's binding slot holds a single output pin
  and a single action, and refuses to overwrite a live one with `ERR_BUSY`. Our
  repeated-flag rule already matches: `--to A5 --to A6` is rejected before
  anything is sent.
- **EXTI lines are shared across ports.** The binding table is indexed by pin
  *number* alone — sixteen slots for the whole chip — because EXTI line N routes
  to pin N of exactly one port at a time. Binding PB5 while PA5 holds the slot
  returns `ERR_BUSY` on a pin the user never touched. This is the single most
  confusing failure in the protocol and the best entry in the hint table.

### timer

- **`get` is not an echo of `cfg`.** Integer prescaler division makes the achieved
  frequency differ from the one requested; that is why the command exists.
- **A timer's frequency is shared by all four of its pins.**
- **`release` freezes pins, it does not silence them.** Stopping the counter
  leaves each pin at whatever level it held. Silence one output with `-u 0`.
- Reconfiguring a live timer fails with `ERR_BUSY` and changes nothing.
- `-f 0` is a range error. Zero is not shorthand for teardown; that is `-c release`.

### pwm

- **`cfg` claims the pin silent**, at 0% until the first `set`, so there is no
  glitch at whatever duty was last there.
- **`release` frees one pin**; the other three in that timer keep running.
- **No PWM command takes `-t`.** Which timer drives a given pin comes from the
  STM32G4 alternate-function table and lives only in the firmware. Frequency is
  addressed by timer, duty by pin, and the MCU resolves the rest.

### link

`-V` reports the CLI and library version only. The protocol carries no version
field by design — host and firmware are built and flashed together — and `probe`
returns only the magic `"MCUO"`. The link cannot tell you what firmware is on the
board, and the help text should say so.

## Validation split

| | |
|---|---|
| **Ours** | Ranges and enum membership only: port A–G, pin 0–15, timer 0–2, freq 1–1000000, duty 0–100. |
| **The MCU's** | Reserved pins, ownership, command ordering, EXTI-line conflicts. They arrive as reason codes and are rendered with a hint. |
| **Never ours** | The pin-to-timer map. This is why no PWM command takes `-t`, and why the `ERR_NOT_INIT` hint below says `-t <0-2>` rather than naming the timer. |

The second and third rows are rules 2 and 1 in `CLAUDE.md`. A second copy of the
MCU's policy here is a second copy that can be wrong.

## Globals

```
      --device <path>     serial port        [MCUCO_DEVICE, then /dev/ttyACM0]
      --timeout <ms>      response deadline  [1000]
  -q, --quiet             no stdout; exit code only
  -h, --help              context-sensitive
  -V, --version
```

`--device` and `--timeout` are long-only because `-d` is direction and `-t` is
timer. `--device` can afford it: `MCUCO_DEVICE` carries it, so it is typed once
per shell. Precedence is flag, then env, then `/dev/ttyACM0`. `--timeout` gets no
env var — a per-shell default for a deadline invites silently slow behaviour — and
`-t 2000` typed by mistake errors with *"did you mean --timeout?"*.

`-h` is generated from the same table that drives dispatch and validation, so it
narrows with what has already been typed and cannot drift from what is
dispatchable:

```
mcu-co-cli -h                  → the five subsystems
mcu-co-cli -s pwm -h           → cfg, set, get, release
mcu-co-cli -s pwm -c set -h    → -p, -u, and their ranges
```

## Exit codes

| code | meaning | source |
|---|---|---|
| `0` | ACK — the MCU did it | `STATUS_OK` |
| `1` | NACK — the MCU refused | firmware `status_t` `1`–`10`, the reason byte |
| `2` | usage — we refused, nothing was sent | `STATUS_ERR_ARG`, bad or missing flags, unknown pair |
| `3` | link — nothing usable came back | `STATUS_ERR_IO` `NO_RESPONSE` `BAD_FRAME` `NOT_OPEN`, failed `mcuco_open` |

**2 means no bytes were ever put on the wire.** `mcuco.h` already sets this up:
`STATUS_ERR_ARG` exists so a local range check is distinguishable from the MCU's
own `ERR_INVALID_ARG`. The exit codes surface that distinction.

`--help` and `-V` exit 0. Nothing exceeds 3, so the shell's 126/127 and the >128
signal range stay clear.

All ten NACK reasons collapse to `1`, with the reason name as the first token on
stderr so it stays greppable. Giving each reason its own code was considered and
rejected: ten more codes to document and keep append-only forever, for a
distinction one `grep` already makes — and the reason list is the firmware's, so
the CLI would be pinning a second copy of a list designed to grow.

`probe` answering with the wrong magic is `3`, not `1`. Something replied, but it
is not mcu-co.

## stdout and stderr

- **stdout is the value and nothing else** — `low`, `high`, `999`, `25.0`. One
  line, no label, no trailing prose.
- **stderr is everything else** — errors, hints, narration.
- **Success with no value prints nothing.** `cfg`, `set`, `release`, `bind`,
  `unbind`, `probe` and `reset` are silent on success, the way `cp` is; exit 0 is
  the confirmation.

That split is what makes both of these work with no parsing and no `-q`:

```sh
level=$(mcu-co-cli -s gpio -c get -p C13) || exit
[ "$level" = high ] && mcu-co-cli -s gpio -c set -p A5 -l high
```

```sh
# claim_group() from examples/pwm_breathe.c, as a shell script
if ! mcu-co-cli -s timer -c cfg -t 0 -f 1000; then
    mcu-co-cli -s timer -c release -t 0
    mcu-co-cli -s timer -c cfg -t 0 -f 1000
fi
```

## Hints

Three of the four failures a hardware user actually hits are "you skipped a setup
step" or "the last run left state behind": `ERR_INVALID_STATE`, `ERR_NOT_INIT` and
`ERR_BUSY`. A bare `FAILED - ERR_INVALID_STATE` is useless; a hint naming the next
command is the single largest usability win available here.

```
$ mcu-co-cli -s gpio -c set -p A5 -l high
mcu-co-cli: ERR_INVALID_STATE: PA5 is not configured as an output
hint: mcu-co-cli -s gpio -c cfg -p A5 -d output

$ mcu-co-cli -s irq -c bind -p B5 -e rising -a high --to A0
mcu-co-cli: ERR_BUSY: EXTI line 5 is already in use
hint: line 5 is shared by pin 5 of every port — something on P?5 holds it

$ mcu-co-cli -s pwm -c cfg -p A5
mcu-co-cli: ERR_NOT_INIT: PA5's timer has no frequency set
hint: mcu-co-cli -s timer -c cfg -t <0-2> -f <hz>
```

This is a `(command, reason_code) → hint` lookup, not a pre-flight gate. The MCU
has already decided; we are rendering its verdict with a suggestion. If a hint is
wrong the user still received the authoritative answer, so no policy is
duplicated.

Two link failures deserve their own message, both still exit 3, because they are
the ones people actually hit:

```
EACCES  mcu-co-cli: /dev/ttyACM0: permission denied
        hint: add yourself to the dialout group, then log out and back in
ENOENT  mcu-co-cli: /dev/ttyACM0: no such device
        hint: set MCUCO_DEVICE, or check the board is plugged in
```

## Vocabulary

The CLI groups by what the user is doing; the wire groups by peripheral. The
mismatch is deliberate and should not be "fixed" later.

| CLI | wire | library |
|---|---|---|
| `-s irq` | `GPIO_IRQ_CFG` / `_BIND` / `_UNBIND` | `mcuco_gpio_irq_*` |
| `-s timer`, `-t N` | `GROUP` — 0/1/2 = TIM2/TIM3/TIM4 | `mcuco_pwm_group_*` |
| `-s pwm` | `PWM_CFG` / `_SET` / `_GET` / `_RELEASE` | `mcuco_pwm_channel_*` |
| `-u` percent | `DUTY`, tenths of a percent | `uint16_t`, tenths |

`gpio` is dropped from `irq` because every EXTI trigger is a GPIO pin — the prefix
adds no information and costs a token on every invocation. `timer`/`pwm` replace
`group`/`channel` because the group *is* a timer, and the two are impossible to
confuse where `group` and `channel` are easy to.

`-u` in percent is an exact bijection onto the wire's tenths — 0–100.0 at one
decimal place is the same set of values as 0–1000 — so nothing is rounded. **OPEN**:
raw tenths is the alternative, trading `-u 25` meaning 2.5% for zero translation.

## Open decisions

1. **`-t/--timer` vs `-n/--number`** for the timer index. `-t` is mnemonic but
   reads as "timeout" in most tools, which is why `--timeout` is long-only.
2. **`-u/--duty` in percent vs raw tenths.** See Vocabulary above.

## Risk to verify on hardware

`configure_port()` in `library/src/uart.c` sets `CLOCAL` but never clears `HUPCL`,
which is on by default, so the kernel drops DTR/RTS when the last fd closes. On a
Nucleo ST-Link VCP those lines are not wired to NRST and nothing should happen —
but on an FTDI cable or a board that wires DTR to reset, **every invocation would
reboot the MCU**, wiping the pin configuration the previous invocation set up.

A one-shot, flag-driven CLI depends entirely on MCU state surviving between
invocations, so this is worth one experiment before building on it:

```sh
mcu-co-cli -s gpio -c cfg -p A5 -d output
mcu-co-cli -s gpio -c set -p A5 -l high     # does this succeed?
```

If the second command returns `ERR_INVALID_STATE`, the direction did not survive
and `HUPCL` needs clearing in `configure_port()`.

## Deferred

- `--dry-run` and `--show-frames`, the Python mock's frame dumping. `protocol.h`
  is public and the `protocol_*` builders are exported, so this stays available;
  it needs either a handler vtable or a second path through all 16 commands, which
  is its own step. Keeping handlers behind a session struct rather than a raw
  `mcuco_t *` preserves the option.
- Device auto-detection when `--device` is absent and exactly one `/dev/ttyACM*`
  exists. A real ease win, but it wants a decision about the two-device case.
- A `1k` / `1M` suffix on `-f`. Genuinely nicer over a six-decade range, and
  additive — it breaks nothing to add later.
