# mcu-co CLI — command-line design

The planned surface for `mcu-co-cli`, a CLI built on `libmcuco.so`.
`mcu-co_Protocol.md` in the firmware repo remains authoritative for the wire
format and is not restated here.

**Status: shape agreed, nothing implemented.** An earlier, flag-based version of
this document was implemented as far as a working argument parser and then
dropped — see *Why this shape* at the end for what that cost and what changed.

## Shape

```
mcu-co-cli <subsystem> <verb> <values...>      [--options anywhere]
```

Two words name the command, then its values in a fixed order. `mcu`, `gpio`,
`irq`, `timer` and `pwm` are the subsystems; each has two to four verbs. Every
command is exactly two words, so a third is a typo rather than something to
ignore, and a missing one is a dropped word rather than a silently incomplete
command.

**Values are positional and value-first.** `gpio set high A 5` — the value, then
the port and pin it applies to. That is the order in `mcuco.h`
(`mcuco_gpio_set(mcu, level, port, pin)`) and the order of the payload bytes on
the wire, so the command you type, the call it makes and the bytes it sends all
read the same direction. `CLAUDE.md` states that rule for the library; with
positional values it is finally true of the CLI too.

**Nothing is optional.** Every command takes a fixed number of words, so the
count alone decides whether a line is well formed; too few or too many prints
that command's one usage line. There is no command whose word count is a range,
and no option that only some commands accept, which is what keeps the parsing to
counting words and converting them.

That costs one thing: `pwm cfg` names its polarity every time, including the
`active-high` that is almost always what you want. It buys the rule holding with
no exceptions - value-first applies to every value, because every value has a
position.

## The 16 commands

```
mcu    probe                                   confirm mcu-co is on the other end
       reset                                   reboot the MCU

gpio   cfg     <input|output> <port> <pin>     set a pin's direction
       set     <low|high> <port> <pin>         drive an output pin
       get     <port> <pin>                    → low | high
       toggle  <port> <pin>                    → low | high   (after the flip)

irq    cfg     <off|rising|falling|both> <port> <pin>
       bind    <rising|falling|both> <port> <pin> <low|high|toggle> <port> <pin>
       unbind  <port> <pin>

timer  cfg     <1-1000000> <0-2>               Hz, then which timer
       get     <0-2>                           → achieved Hz
       release <0-2>

pwm    cfg     <active-high|active-low> <port> <pin>
       set     <0-100> <port> <pin>            percent, then the pin
       get     <port> <pin>                    → percent, one decimal
       release <port> <pin>
```

`mcu` is a subsystem like any other rather than `mcu-co-cli probe` standing
alone, so every command is two words and the word count is exact. It is named
for the chip rather than the connection because that is what its verbs address:
`probe` tests the link, but `reset` reboots the MCU.

A `(subsystem, verb)` pair outside this table is rejected, naming the verbs that
subsystem does take.

`irq bind` is the one command with six values, and they read as two triples:
*on `rising` of `B 5`, drive `high` on `A 0`.*

```
mcu-co-cli irq bind rising B 5 high A 0
```

## Values

| value | accepts |
|---|---|
| port | `A`–`G`, capitals only |
| pin | `0`–`15` |
| direction | `input` `output` |
| level | `low` `high` |
| edge | `off` `rising` `falling` `both` — `off` refused by `bind` |
| action | `low` `high` `toggle` |
| timer | `0` `1` `2` |
| frequency | `1`–`1000000`, plain integer Hz |
| duty | `0`–`100`, plain integer percent |

**Port and pin are two words.** `A 5`, not `A5`. Each word is then one value
with one check, the same as every other row in this table, rather than a word
that has to be split into a letter and a number first. The port has exactly one
spelling per port - a single capital letter - so `a`, `PA` and `pa` are bad
ports, not aliases.

A bad value is one message naming the value and the command's usage line:

```
$ mcu-co-cli gpio set hgih A 5
mcu-co-cli: bad level 'hgih'
usage: mcu-co-cli gpio set <low|high> <port> <pin>
```

That one line covers a bad value, a missing value and an extra value, which is
why there is no separate wording for each.

Duty is percent, not the wire's tenths of a percent. Raw tenths was the
alternative and was rejected: `25` is a legal value there, meaning 2.5%, wrong
by a factor of ten with nothing to flag it. Whole percent maps onto tenths
exactly — it is just coarser — and accepting one decimal place later is
additive, since `25` keeps meaning 25%.

There is no batch form; the protocol is one command per pin.

## Options

Only five, and **every one of them applies to every command**:

```
  -d, --device <path>       serial port        [MCUCO_DEVICE, then /dev/ttyACM0]
  -t, --timeout <ms>        response deadline  [1000]
  -v, --verbose <n>         log level
  -h, --help
  -V, --version
```

No option belongs to one command, and no value is ever spelled as an option, so
there is no flag table to keep in step with the command table and no short-letter
collisions to design around.

Options may appear anywhere on the line - before the command words, after them,
or between them. `getopt_long` lifts them out and leaves the plain words in the
order they were typed, so all three of these are the same command:

```sh
mcu-co-cli gpio set high A 5 --timeout 50
mcu-co-cli --timeout 50 gpio set high A 5
mcu-co-cli gpio set --timeout 50 high A 5
```

Only the order of the plain words among themselves carries meaning.

`--device` precedence is option, then env, then `/dev/ttyACM0`. That default
suits an ST-Link virtual COM port; an FTDI or CP210x adapter enumerates as
`/dev/ttyUSB0` and the i.MX93's own UART is different again, which is the reason
the env var exists rather than a longer list of defaults. `--timeout` gets no
env var — a per-shell default for a deadline invites silently slow behaviour.

`-v <n>` sets the logger's verbosity; only messages at or below that level
print. All of it goes to stderr, so `-v` never changes what a script captures
from stdout.

`--help` prints **one screen**. Help that narrows as you type was considered and
rejected: sixteen commands fit on a screen, and three scopes of generated help
cost more code than the whole of the parsing.

### What `--help` prints

48 lines, 78 columns, so it does not wrap on an 80-column terminal.

```
usage: mcu-co-cli <subsystem> <verb> <values...> [--options anywhere]

  mcu    probe                                check the link
         reset                                reboot the MCU

  gpio   cfg      <dir> <port> <pin>          set a pin's direction
         set      <level> <port> <pin>        drive an output pin
         get      <port> <pin>                -> low | high
         toggle   <port> <pin>                -> the level after the flip

  irq    cfg      <edge> <port> <pin>         arm or disarm a trigger
         bind     <edge> <port> <pin> <action> <port> <pin>
                                              drive one pin from another
         unbind   <port> <pin>                drop the action, stay armed

  timer  cfg      <1-1000000> <0-2>           Hz, then which timer
         get      <0-2>                       -> achieved Hz
         release  <0-2>                       stop it, freezing its pins

  pwm    cfg      <polarity> <port> <pin>     claim a pin, silent at 0%
         set      <0-100> <port> <pin>        percent, then the pin
         get      <port> <pin>                -> percent, one decimal
         release  <port> <pin>                free one pin

  dir       input | output
  level     low | high
  edge      off | rising | falling | both     off is invalid for bind
  action    low | high | toggle
  polarity  active-high | active-low
  port      A-G, capitals only
  pin       0-15

pwm pins, by timer (from firmware peripherals/timer.c):
  timer 0   TIM2    A5     A1     B10    B11
  timer 1   TIM3    C6     C7     C8     C9
  timer 2   TIM4    B6     B7     B8     B9

  Reference only. The MCU resolves which channel a pin is and refuses a pin
  with none; no command takes a timer for a pwm operation.

options, valid on every command and in any position:
  -d, --device <path>    serial port    [$MCUCO_DEVICE, then /dev/ttyACM0]
  -t, --timeout <ms>     deadline       [1000]
  -v, --verbose <n>      log level
  -h, --help
  -V, --version

exit: 0 done   1 the MCU refused   2 bad command, nothing sent   3 link failed
```

Each command line reads word for word as it is typed: `<port> <pin>` is two
placeholders because it is two words. The values a named placeholder accepts -
`<dir>`, `<level>`, `<edge>`, `<action>`, `<polarity>` - live in the legend
below the commands rather than on every line, which is what leaves room for the
descriptions. `irq bind` is still too wide for one line, so its description
wraps onto the next.

**`--help` asked for goes to stdout and exits 0**; the usage text printed after
a bad command goes to stderr and exits 1. That keeps `mcu-co-cli --help | less`
working while leaving `level=$(mcu-co-cli gpio get C 13)` free of help text,
which is the same split the rest of this document relies on.

### Why the pin map is in there

Without this, `pwm cfg` is unusable without the firmware source open: the only
way to learn that PA5 needs `timer cfg <hz> 0` is to read
`timer_pins[][]`, and `ERR_NOT_INIT` cannot say which timer to configure.

**This is a bounded exception to rule 1 in `CLAUDE.md`, and the bound is what
makes it safe:** the table is printed and nothing else. No code path reads it to
choose a group, to validate a pin, or to decide whether a `pwm` command can
succeed. Frequency is still addressed by timer and duty by pin, and the MCU still
resolves which channel a pin is, refusing a pin with no PWM channel with
`ERR_UNSUPPORTED`. So a stale copy here gives bad advice on one screen; it can
never send a wrong frame or refuse a valid one, which is what rule 1 exists to
prevent.

It *can* go stale. The map is firmware policy, not silicon: the STM32G4
alternate-function table offers TIM2_CH1 on PA0, PA5 and PA15, and the firmware
picked PA5. If the firmware repoints a channel, this screen is wrong until it is
edited. Two ways out, both deferred below: generate the block from
`peripherals/timer.c` at build time, or add a protocol command so the board can
be asked.

The `ERR_NOT_INIT` hint still says `<0-2>` rather than naming the pin's timer.
That is the line between the two: printing the whole map is a reference the user
reads, while naming *this pin's* timer would be the CLI resolving the map, which
is the thing rule 1 forbids.

`-V` reports the CLI and library version only. The protocol carries no version
field by design — host and firmware are built and flashed together — and `probe`
returns only the magic `"MCUO"`. The link cannot tell you what firmware is on
the board, and the help text should say so.

## Per-subsystem notes

Things a user will otherwise get wrong. These belong in the help text.

### gpio

`toggle` prints the level it ended up at, taken from the response DATA. It is
free, and it saves a `get` round trip.

### irq

- **The edge is typed twice on `bind` by design.** The firmware requires the
  bound edge to match the armed edge exactly, because EXTI cannot report which
  edge fired. It is a confirmation, not a second setting; a mismatch is
  `ERR_INVALID_STATE`.
- **`unbind` and `cfg off` are different teardowns.** `unbind` drops the action
  and leaves the trigger armed. `cfg off` disarms the pin *and* clears its
  bindings.
- **One output per input.** The firmware's binding slot holds a single output
  pin and a single action, and refuses to overwrite a live one with `ERR_BUSY`.
- **EXTI lines are shared across ports.** The binding table is indexed by pin
  *number* alone — sixteen slots for the whole chip — because EXTI line N routes
  to pin N of exactly one port at a time. Binding PB5 while PA5 holds the slot
  returns `ERR_BUSY` on a pin the user never touched. This is the single most
  confusing failure in the protocol and the best entry in the hint table.

### timer

- **`get` is not an echo of `cfg`.** Integer prescaler division makes the
  achieved frequency differ from the one requested; that is why the command
  exists.
- **A timer's frequency is shared by all four of its pins.**
- **`release` freezes pins, it does not silence them.** Stopping the counter
  leaves each pin at whatever level it held. Silence one output with a duty of
  `0`.
- Reconfiguring a live timer fails with `ERR_BUSY` and changes nothing.
- A frequency of `0` is a range error. Zero is not shorthand for teardown; that
  is `timer release`.

### pwm

- **`cfg` claims the pin silent**, at 0% until the first `set`, so there is no
  glitch at whatever duty was last there. The polarity is named on every `cfg`
  rather than defaulted, so what "0%" means on the wire is never implicit.
- **`release` frees one pin**; the other three in that timer keep running.
- **No pwm command names a timer.** Which timer drives a given pin comes from
  the STM32G4 alternate-function table and lives only in the firmware. Frequency
  is addressed by timer, duty by pin, and the MCU resolves the rest. `--help`
  prints the map so the user can do the lookup themselves; no command takes it as
  an argument.
- **Twelve pins have a PWM channel, out of the whole chip.** Four per timer. A
  pin outside that set is `ERR_UNSUPPORTED`, not `ERR_INVALID_PIN` — it is a real
  pin with no channel behind it.

## Validation split

| | |
|---|---|
| **Ours** | Ranges and word membership only: port A–G, pin 0–15, timer 0–2, frequency 1–1000000, duty 0–100. |
| **The MCU's** | Reserved pins, ownership, command ordering, EXTI-line conflicts. They arrive as reason codes and are rendered with a hint. |
| **Never ours** | The pin-to-timer map, as an input to any decision. This is why no pwm command names a timer, and why the `ERR_NOT_INIT` hint says `<0-2>` rather than naming the pin's timer. `--help` prints the map as reference text only — see *The PWM pin map in `--help`* for why that is not the same thing. |

The second and third rows are rules 2 and 1 in `CLAUDE.md`. A second copy of the
MCU's policy here is a second copy that can be wrong.

## Exit codes

| code | meaning |
|---|---|
| `0` | it worked |
| `1` | it did not |

That is the whole scheme. `--help` and `-V` exit `0`; every failure exits `1`,
whether the MCU refused it, the link died, or the command was typed wrong.

An earlier version of this document split failure four ways — `1` NACK, `2`
usage, `3` link — so that a script could tell "the MCU said no" from "you typed
it wrong". It was dropped because almost nothing needs that distinction and
`mcuco_strerror()` already prints it on stderr, where a human reads it and a
script can `grep` it. Two codes are also two fewer things to keep append-only
forever.

The shell patterns in *stdout and stderr* below never needed more than two:
both test for non-zero.

What is *lost* is worth naming, in case it is ever wanted back. `STATUS_ERR_ARG`
exists in `mcuco.h` precisely so a local range check is distinguishable from the
MCU's own `ERR_INVALID_ARG`, and with one failure code the shell can no longer
see that difference. The guarantee itself still holds — every value is converted
before `mcuco_open` is called, so a rejected command sends nothing — it is just
no longer reported through the exit status. Reinstating a separate code for it is
additive: `0` and `1` keep their meanings.

Nothing exceeds `1`, so the shell's 126/127 and the >128 signal range stay clear.

## stdout and stderr

- **stdout is the value and nothing else** — `low`, `high`, `999`, `25.0`. One
  line, no label, no trailing prose.
- **`pwm get` prints one decimal place**, always, because the wire holds tenths
  of a percent and something else may have set a value this CLI cannot type.
  Printing `25` for 255 tenths would be a quiet lie; `25.5` is what is there.
  This is the one place input and output are not symmetric, and it is why
  accepting a decimal on `set` later is additive rather than a fix.
- **stderr is everything else** — errors, hints, narration.
- **Success with no value prints nothing.** `cfg`, `set`, `release`, `bind`,
  `unbind`, `probe` and `reset` are silent on success, the way `cp` is; exit 0
  is the confirmation.

That split is what makes both of these work with no parsing:

```sh
level=$(mcu-co-cli gpio get C 13) || exit
[ "$level" = high ] && mcu-co-cli gpio set high A 5
```

```sh
# claim_group() from examples/pwm_breathe.c, as a shell script
if ! mcu-co-cli timer cfg 1000 0; then
    mcu-co-cli timer release 0
    mcu-co-cli timer cfg 1000 0
fi
```

There is no `--quiet`. Success is already silent and stdout is already only the
value, so the only thing it could suppress is the output of four commands, which
`>/dev/null` suppresses already. `-v` is the other direction and does earn its
place: what it adds on stderr exists nowhere else.

## Hints

Three of the four failures a hardware user actually hits are "you skipped a
setup step" or "the last run left state behind": `ERR_INVALID_STATE`,
`ERR_NOT_INIT` and `ERR_BUSY`. A bare `FAILED - ERR_INVALID_STATE` is useless; a
hint naming the next command is the single largest usability win available here.

```
$ mcu-co-cli gpio set high A 5
mcu-co-cli: ERR_INVALID_STATE: the pin is not configured for this operation
hint: configure it first, e.g. mcu-co-cli gpio cfg output A 5

$ mcu-co-cli irq bind rising B 5 high A 0
mcu-co-cli: ERR_BUSY: the resource is already in use
hint: EXTI line N is shared by pin N of every port — something on P?5 holds it

$ mcu-co-cli pwm cfg active-high A 5
mcu-co-cli: ERR_NOT_INIT: the peripheral was never brought up
hint: give the pin's timer a frequency first, e.g. mcu-co-cli timer cfg 1000 <0-2>
```

**Keyed on the reason code alone**, not on `(verb, reason)`. Three reasons,
three hints. The reason is what carries the information; the verb only changed
the wording, and a table indexed by both is sixteen times the entries for that.

This is a lookup, not a pre-flight gate. The MCU has already decided; we are
rendering its verdict with a suggestion. If a hint is wrong the user still
received the authoritative answer, so no policy is duplicated.

Three link failures deserve their own message, all exiting 1 like any other
failure, because they are the ones people actually hit:

```
EACCES  mcu-co-cli: /dev/ttyACM0: permission denied
        hint: add yourself to the dialout group, then log out and back in
ENOENT  mcu-co-cli: /dev/ttyACM0: no such device
        hint: set MCUCO_DEVICE, or check the board is plugged in
EBUSY   mcu-co-cli: /dev/ttyACM0: in use by another process
        hint: another mcu-co-cli or a terminal program holds the port
```

`EBUSY` is the port being held: `configure_port()` in `library/src/uart.c` sets
`TIOCEXCL`, so a second open fails rather than handing out a descriptor that
reads the first one's replies. Responses carry no opcode, so that theft would be
undetectable.

## Vocabulary

The CLI groups by what the user is doing; the wire groups by peripheral. The
mismatch is deliberate and should not be "fixed" later.

| CLI | wire | library |
|---|---|---|
| `irq` subsystem | `GPIO_IRQ_CFG` / `_BIND` / `_UNBIND` | `mcuco_gpio_irq_*` |
| `timer` subsystem, `0`–`2` | `GROUP` — 0/1/2 = TIM2/TIM3/TIM4 | `mcuco_pwm_group_*` |
| `pwm` subsystem | `PWM_CFG` / `_SET` / `_GET` / `_RELEASE` | `mcuco_pwm_channel_*` |
| duty percent | `DUTY`, tenths of a percent | `uint16_t`, tenths |

`gpio` is dropped from `irq` because every EXTI trigger is a GPIO pin — the
prefix adds no information and costs a word on every invocation. `timer`/`pwm`
replace `group`/`channel` because the group *is* a timer, and the two are
impossible to confuse where `group` and `channel` are easy to.

## MCU state between invocations

**Confirmed: MCU state survives the port closing.** A one-shot CLI is therefore
sound — `gpio cfg` in one invocation and `gpio set` in the next is a valid
sequence, and every multi-step workflow in this document works as written.

What `TIOCEXCL` does *not* protect is a sequence. Exclusion is per invocation,
so two shells can each hold the port cleanly in turn and still interleave
`gpio cfg output A 5` / `gpio cfg input A 5` / `gpio set high A 5`. The MCU is
shared mutable state with no concept of which shell asked. The firmware refuses
the dangerous clobbers — a live timer or an occupied binding slot returns
`ERR_BUSY` — and that is the only backstop there can be. **The link is
single-user and enforced; the board is not.** The help text should say so.

One thing to remember if state ever stops surviving. `configure_port()` sets
`CLOCAL` but never clears `HUPCL`, which is on by default, so the kernel drops
DTR/RTS when the last fd closes. That is harmless on hardware that does not wire
DTR to NRST. On hardware that does, every invocation would reboot the MCU and
wipe the previous one's configuration. If a sequence that used to work starts
failing with `ERR_INVALID_STATE` on a different board or cable, check this
first; the fix is one line alongside the other `c_cflag` settings:

```c
tty.c_cflag &= (tcflag_t)~HUPCL;
```

`cfmakeraw()` does not clear it, which is why it is not already there.

## Why this shape

The first version of this document put every argument behind a flag:
`gpio set -p A5 -l high`, with sixteen commands each declaring which flags it
took. It was implemented as far as a working parser — about 750 lines — before
being dropped. Where those lines went is the reason for the shape above.

Only about 90 lines read the command line. Roughly 300 printed text: help that
narrowed as you typed, four different messages for a malformed pin, and the
valid set listed on every bad value. The rest was flag bookkeeping that exists
only because flags can be typed in any order — accepting them in any order,
catching a repeat, knowing which flags each command allows, and naming the one
that is missing.

Positional values delete all four of those jobs at once: the program counts
words. One help screen and one message per failure delete most of the rest. Same
sixteen commands, roughly a third of the code, and one fewer class of bug — the
worst defect found while building the flag version was getopt state leaking
between two parses in one process, which cannot happen without getopt.

What was kept from the first version, because it was cheap and carried its
weight: two words for subsystem and verb, an exact word count, exit codes
`0`–`3` with *2 means nothing reached the wire*, the stdout/stderr split, and
the validation split against the MCU's.

## Deferred

- **Generating the PWM pin map instead of typing it.** A build step reading
  `timer_pins[][]` out of `../mcu-co_firmware/peripherals/timer.c` when that
  checkout is present, falling back to a committed copy. Removes the drift, and
  costs a parser for one C array plus a decision about what to do when the board
  runs older firmware than the checkout.
- **A protocol command for the pin map.** The real fix, and the only one that
  answers for the firmware actually flashed: the board reports its own map.
  Needs a new opcode, so it is a firmware and protocol decision, not a host one.
- **One decimal place on duty.** `25.5` meaning 25.5%. Additive: `25` keeps
  meaning 25%.
- **A `1k` / `1M` suffix on frequency.** Genuinely nicer over a six-decade
  range, and additive.
- **`--dry-run` and `--show-frames`**, the Python mock's frame dumping.
  `protocol.h` is public and the `protocol_*` builders are exported, so this
  stays available; it needs either a handler vtable or a second path through all
  16 commands. Keeping handlers behind a session struct rather than a raw
  `mcuco_t *` preserves the option.
- **Device auto-detection** when `--device` is absent and exactly one
  `/dev/ttyACM*` exists. A real ease win, but it wants a decision about the
  two-device case.
- **`--wait`**, retrying the open when the port is held, rather than failing
  with `EBUSY` immediately.
