---
name: tdd
description: Build a new module test-first, following Test-Driven Development for Embedded C. Use when the user asks to start a new module, write something test-first, or invokes /tdd. Enforces one test at a time, red before green, and single-step pacing.
user-invocable: true
---

# Test-first module development

Follow *Test-Driven Development for Embedded C*'s cycle. This is a pacing
discipline as much as a testing one: the point is small verified steps, not
speed.

## 1. Write a test list before any test code

A plain-language checklist of scenarios per function, saved as
`library/tests/<module>_test_list.md`. Not a spec — cross items off, or add new
ones, as work proceeds.

Flag any open design question directly in the list rather than guessing, and
say what it blocks. Resolve it in discussion before writing tests against it.

Stop here and wait. The list is the deliverable for this step.

## 2. One test at a time, red then green

Write exactly one failing test. Build it and confirm it fails **for the expected
reason** — compile error, link error, or assertion failure, not just "some"
error. A test that fails because of a typo in the test proves nothing.

Then write the *minimal* implementation to pass that one test, not code that
anticipates tests not yet written. Confirm green before moving on.

## 3. Default to single-step pacing

Stop after each individual step — write test, confirm red, write implementation,
confirm green — and wait for confirmation rather than chaining the whole cycle
unprompted.

Even if a prior message said "start on X," that authorizes one step, not the
rest of the list.

The user may explicitly grant broader autonomy ("make the rest of the module
with this approach") to proceed through remaining list items without pausing.
That grant does not carry over to the next module or session by default.

## 4. Extending a fake is its own step

If a test needs more from a fake than it currently exposes, add that capability
first, confirm it doesn't break existing tests, *then* write the new test
against it. Don't bundle a fake change into the step that needs it.

## 5. Interface and protocol changes get discussed, not assumed

If implementing a test reveals a real gap or ambiguity in the wire protocol or a
locked-in interface, stop and raise it rather than silently extending the
design. These are collaborative decisions.

## Conventions this project already uses

- CppUTest, one `unit_tests` binary via `AllTests.cpp`
- `TEST(GroupName, BehaviorDescription)` — state the guarantee, not the
  implementation: `ReadDeadlineIsNotResetByAPartialRead`, not `TestReadLoop`
- Group cases under `/* --- function_name --- */` per production function
- Real frames from `mcu-co_Protocol.md` as test data, never invented bytes
- `openpty()` stands in for the board; open the slave through `uart_open` so the
  port is configured raw
