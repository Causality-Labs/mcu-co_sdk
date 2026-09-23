# Test list — `cli/args.c`, pin token parsing

Scenarios for the first function in the CLI's argument module. Grammar and
error wording come from `cli/Plans.md`.

## Function under test

```c
/* Parses a CLI pin token into its wire fields. `port` and `pin` are untouched
 * unless STATUS_OK is returned. */
mcu_status_t args_parse_pin(const char *token, port_t *port, uint8_t *pin);
```

Assumed, not yet agreed — see open question 2 for the return type.

## Not this function's job

Reserved pins (PA13–15, PB3/4) are **not** checked here. That is ownership
policy, which rule 2 puts on the MCU; it arrives as `ERR_INVALID_PIN`. This
function validates ranges only: port A–G, pin 0–15.

## Open questions — resolve before writing tests against them

**1. How CLI tests link.** Tests live here, in `cli/tests/`. What is still
undecided is the target: `cli/` is not in the root `CMakeLists.txt` at all, so
before the first test can compile it needs either its own test binary, or its
sources added to the existing `unit_tests` — `CLAUDE.md` says all test files
link into one `unit_tests` binary via `AllTests.cpp`, which argues for the
second. Either way `cli/` needs a target holding `args.c` separately from
`main.c`, so a test can link the parsing without linking a `main()`.
*Blocks: every test below.*

**2. How the parser reports which half failed.** `cli/Plans.md` promises four
distinct messages — *port must be A–G*, *pin must be 0–15*, *expected
`<port><pin>`*, *missing pin number* — but `mcu_status_t` has a single local
code, `STATUS_ERR_ARG`. Options: an out-param reason enum, a caller-supplied
message buffer, or dropping the distinction and rendering one generic message.
*Blocks: every rejection test that asserts on which error.*

**3. What `"P5"` means.** If the leading `P` is stripped unconditionally, this
becomes `"5"` — a missing port letter. If it is not, `P` is an out-of-range port.
Both are rejections; they differ only in the message. *Blocks: one test.*

## Accepted forms

- [ ] `"A5"` yields `PORT_A` and pin 5
- [ ] `"a5"` — lowercase port letter yields the same
- [ ] `"PA5"` — optional leading `P` is accepted
- [ ] `"pa5"` — lowercase prefix and port
- [ ] `"A05"` — leading zero in the pin yields 5
- [ ] `"A0"` — lowest pin
- [ ] `"A15"` — highest pin
- [ ] `"G0"` — highest port letter yields `PORT_G`
- [ ] every letter A–G maps to `PORT_A`–`PORT_G` in order

## Rejected forms

- [ ] `"H0"` — port past G
- [ ] `"A16"` — pin past 15
- [ ] `"A"` — port with no pin number
- [ ] `"5A"` — digit before letter
- [ ] `""` — empty token
- [ ] `"A5x"` — trailing junk after a valid pin
- [ ] `"A-1"` — negative pin
- [ ] `"A99999999999999999999"` — pin that overflows the parse
- [ ] `"P5"` — see open question 3
- [ ] `NULL` token

## Contract

- [ ] `port` and `pin` are untouched when the token is rejected
- [ ] a `NULL` `port` or `pin` is rejected rather than dereferenced

## Priority

Per the project's testing note — guard the documented contract plus one happy
path before exhaustive edges. First four, in order:

1. `"A5"` yields `PORT_A` and pin 5
2. `port` and `pin` untouched on rejection
3. `"A16"` rejected — the range check that is this function's actual job
4. `"H0"` rejected — the other half of the range check

The remaining accepted forms and the malformed-token cases follow once those
four hold.
