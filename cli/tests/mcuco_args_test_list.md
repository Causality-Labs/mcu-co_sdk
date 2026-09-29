# Test list — `cli/mcuco_args.c` — all sixteen commands parse

`mcu`, `gpio` and `timer` are done and covered by `cli/tests/test_mcuco_args.cpp`.

Since the table rewrite there are no per-subsystem `assign_*` functions. Adding
a command is:

1. a `WORD_*` kind for each new value, with a field in `struct mcuco_args`
2. a `parse_*` function for each new kind, and its case in `parse_value`
3. a row in `commands[]` in `mcuco_command.c`, with its `value_count`

The run handler stays `run_not_wired` here; calling the library is
`mcuco_command`'s job and a separate step.

All tests go through `args_parse_mcuco` — the parsers stay static. Every row
needs at least one test that parses it successfully: that is what catches a
wrong `value_count`.

## Decided

- **Duty is whole percent**, `0`–`100`, times ten into `duty_tenths`. `25.5` is
  rejected rather than truncated; one decimal place is in `cli/Plans.md`'s
  Deferred list.
- **Polarity is spelled `active-high` / `active-low`.** The protocol's own `POL`
  column says `high|low`, but those are level words here, and accepting them for
  polarity would make `pwm cfg high A 5` mean something unrelated to `gpio set
  high A 5`.
- **Numbers start with a digit**, as `parse_pin` and `parse_timer` already
  require: `strtoul` would otherwise take `+25`, ` 25` and an empty word.
- **`off` is a valid edge for `irq cfg` but not for `irq bind`.** The one
  per-command value restriction, so it needs its own check after parsing.

## pwm — done

```
pwm   cfg      <active-high|active-low> <port> <pin>    3 values
      set      <0-100> <port> <pin>                     3
      get      <port> <pin>                             2
      release  <port> <pin>                             2
```

New kinds: `WORD_POLARITY`, `WORD_DUTY`. New fields: `polarity`, `duty_tenths`.

### Rows
- [x] `pwm get A 5` reaches the port and pin
- [x] `pwm release A 5` reaches the port and pin
- [x] `pwm cfg active-low A 5` reaches the polarity, port and pin
- [x] `pwm set 25 A 5` reaches 250 tenths, port and pin

### Polarity
- [x] `active-low` yields `POL_ACTIVE_LOW`
- [x] `active-high` yields `POL_ACTIVE_HIGH`
- [x] `high`, `low` and `ACTIVE-LOW` are rejected

### Duty
- [x] `0` yields 0 tenths, `100` yields 1000
- [x] `101` is rejected
- [x] `25x` is rejected — the whole word must be a number
- [x] `25.5` is rejected rather than truncated
- [x] `+25` and an empty word are rejected
- [x] `-1` is rejected rather than wrapping

### Shape
- [x] `pwm cfg A 5` is rejected — the polarity is required, not defaulted
- [x] `pwm set A 5` is rejected — the duty is missing
- [x] every pwm verb is rejected one word short and one word long
- [x] `pwm toggle A 5` and `pwm bind 25 A 5` are rejected
- [x] `pwm set 25 A 5 0` is rejected — no pwm command takes a timer

## irq — done

```
irq   cfg      <off|rising|falling|both> <port> <pin>                    3 values
      bind     <rising|falling|both> <port> <pin> <low|high|toggle> <port> <pin>   6
      unbind   <port> <pin>                                              2
```

New fields: `edge`, `action`, `out_port`, `out_pin`.

New kinds:

- `WORD_EDGE` — `off`, `rising`, `falling`, `both`, for `irq cfg`
- `WORD_BIND_EDGE` — the same minus `off`, for `irq bind`
- `WORD_ACTION` — `low`, `high`, `toggle`
- `WORD_OUT_PORT`, `WORD_OUT_PIN` — the second pin of `bind`, so it lands in
  `out_port` / `out_pin` instead of overwriting the trigger pin

**Why a second edge kind.** `off` is legal for `cfg` and illegal for `bind`,
because EXTI cannot report which edge fired and a binding has to name a real one.
Giving `bind` its own word kind puts that rule in the table like every other
rule, instead of a special case in `args_parse_mcuco` that checks which command
it is parsing.

### Rows
- [x] `irq unbind B 5` reaches the port and pin
- [x] `irq cfg rising B 5` reaches the edge, port and pin
- [x] `irq bind rising B 5 high C 7` puts B 5 in the trigger pin and C 7 in the
      output pin, not the other way round

### Edge
- [x] `rising`, `falling`, `both` are accepted by `cfg`
- [x] `off` is accepted by `cfg` — that is the disarm
- [x] `off` is rejected by `bind`
- [x] `RISING`, `rise` and `none` are rejected

### Action
- [x] `low`, `high`, `toggle` are accepted
- [x] `on`, `HIGH` and `flip` are rejected

### Shape
- [x] a bad value in either of `bind`'s pins is rejected
- [x] every irq verb is rejected one word short and one word long
- [x] `irq set high B 5` and `irq get B 5` are rejected

## Last — done

- [x] one example line per command in the table, all sixteen, each parses —
      the guarantee that every row has a passing parse, so a wrong
      `value_count` can never go unnoticed
