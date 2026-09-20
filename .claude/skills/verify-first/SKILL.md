---
name: verify-first
description: How to plan, implement and explain work in this repo. Prove claims by running them rather than asserting, confirm edits landed, and explain with real output and worked examples. Use when implementing a feature, debugging, or explaining how something works.
user-invocable: true
---

# Verify first

The thread through all three phases: **a claim you have not run is a guess**.
Most of the expensive mistakes in this repo were things that looked right.

## Planning

**Read the authoritative source before designing.** `mcu-co_Protocol.md` and the
firmware are the spec. Reading them first turns design questions into lookups —
the frame bytes, the NACK reasons and the ordering rules are all already decided.

**Flag open questions, and say what each blocks.** A question that blocks one
test is worth noting; a question that blocks every test is worth stopping for.
Do not guess past either.

**Recommend one option.** Give the trade-off in a sentence, then say which and
why. A survey of four approaches is work handed back, not work done.

**Keep plans short.** A plan long enough to need its own review is too long.

## Implementing

**Run the thing that would prove you wrong.**

- A test that passes might be passing for the wrong reason. `ProbeRejectsAValidFrameCarryingTheWrongMagic`
  passed on a hand-written CRC that turned out invalid — the frame was rejected
  on the checksum and never reached the magic comparison. Both paths return
  `STATUS_ERR_BAD_FRAME`, so it looked fine. Stub out the thing under test and
  confirm the test then fails.
- A loop that terminates might terminate by luck. The brightness ramp only
  ended because its step divided the range exactly; any other step ran off.

**Confirm the edit landed.** `sed` and scripted replacements fail silently when
the anchor text has changed. The TIOCEXCL fix "didn't work" twice before a
`grep` showed it had never been applied. After any scripted edit, grep for the
result.

**Check the tool did work.** A clean run is not the same as a run that checked
something. `-s both` reported success while compiling zero files, because
analysis only runs during compilation and the tree was warm.

**Distinguish a break from a race.** Two tests started failing right after an
unrelated change; the change was innocent and the suite had been flaky for
several turns. Re-run several times before believing a failure — or a pass.

**Use real data.** Frames come from the protocol doc, never invented. Where a
case the doc does not cover is needed, compute it and say so in a comment.

**One step at a time.** Deliver the step asked for. Install rules, visibility
control and a second module are separate steps with their own approval.

## Explaining

**Show the output, don't describe it.** Paste the bytes, the test names, the
measured timing. `0 files analysed / 9 files analysed` settles an argument that
paragraphs would not.

**Use a worked example with real numbers.** Endianness is abstract; `1000 Hz ->
0x000003E8 -> E8 03 00 00` is not. Pick the example from this project, not a
generic one.

**Name what was given up.** Every choice costs something: the generalised endian
helper lost compile-time width checking; probing on open doubled the suite's
runtime. State it in a line rather than presenting a change as free.

**Say plainly what is still wrong.** Leaked handles on the error paths, a stale
comment, a test that only covers one direction. Ending with the known gaps is
more useful than ending with a summary of what works.

**Correct yourself directly.** "That test was passing for the wrong reason" —
then the fix. No preamble, no recap of how it happened.
