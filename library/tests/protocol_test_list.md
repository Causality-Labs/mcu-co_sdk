# protocol.c — test list

Pure encode/decode for the wire protocol. No I/O, no file descriptors: bytes in,
bytes out, so all of it runs with no device attached.

Authoritative source is `../mcu-co_firmware/mcu-co_Protocol.md`. Its worked examples
are valid frames and are used verbatim as test data — nothing here invents bytes.

```
command   SOF · OPCODE · LEN · PAYLOAD · CRC_L · CRC_H      5 + LEN bytes
response  SOF · LEN · ACK/NACK [ · DATA ] · CRC_L · CRC_H   4 + LEN bytes
```

Constants come from the firmware's `frame_parser.h`: `RX_MAX_PAYLOAD` 32,
`TX_DATA_MAX` 4, `TX_FRAME_MAX` 9, SOF `0xA5`.

---

## Open questions — resolve before writing tests against them

1. **Where does the response struct live?** `parse_response` needs somewhere to put
   `ack` + `data`. Options: a `protocol.h`-private struct, or a public one in
   `mcuco.h`. Leaning private — the public API returns a status and an out-param
   value, so callers never see a frame. **Blocks:** every `parse_response` test.

2. **What does protocol.c return on error?** `mcuco.h` is empty, so there is no
   status enum yet. Options: (a) define `mcuco_status_t` now, (b) use negative
   errno like `uart.c`, (c) a protocol-local enum. Leaning (a) — the NACK reason
   byte *is* a firmware `status_t`, so the public enum has to exist anyway and
   inventing a second vocabulary here means translating twice.
   **Blocks:** error-case tests for both functions.

3. **Does `crc16` get its own file?** The firmware keeps `common/crc16.c` separate
   from `frame_parser.c`. Mirroring that means `crc16.c` + `crc16_test_list.md`
   as its own module first. Leaning yes — it is independently testable and the
   split already exists on the other side.

4. **Signature of the CRC over a frame.** Firmware takes `uint8_t data_size`
   (max 255). Host frames are at most 37 bytes, so `size_t` costs nothing and
   removes a truncation trap. No reason to copy the narrow type.
   Leaning `size_t`.

---

## crc16_ccitt

CCITT-FALSE: poly `0x1021`, init `0xFFFF`, no reflection, no final XOR.

- [ ] Known-answer: the CRC of a doc frame's covered bytes equals the CRC the doc
      prints — e.g. `30 03 01 00 05` gives `0xE1AB`, transmitted `AB E1`.
- [ ] Empty input returns the bare `0xFFFF` seed.
- [ ] A single-bit flip anywhere in the input changes the result.
- [ ] Same bytes always give the same result (no dependence on prior calls —
      guards against a stateful implementation).
- [ ] Byte order on the wire is little-endian: CRC `0xE433` is sent `33 E4`.

## build_command

- [ ] Builds each of the 13 opcodes' doc frames byte-for-byte. Full list to cross
      off as they land:
  - [ ] `GPIO_CFG` — `A5 30 03 01 00 05 AB E1`
  - [ ] `GPIO_WRITE` — `A5 31 03 01 00 05 FA 4B`
  - [ ] `GPIO_READ` — `A5 32 02 00 05 84 7B`
  - [ ] `GPIO_IRQ_BIND` — `A5 33 06 03 02 0D 02 00 05 92 93`
  - [ ] `GPIO_IRQ_CFG` — `A5 34 03 03 00 05 CD 06`
  - [ ] `GPIO_IRQ_CFG` off — `A5 34 03 00 00 05 9D 5F`
  - [ ] `GPIO_IRQ_UNBIND` — `A5 35 02 00 05 A9 2A`
  - [ ] `PWM_GROUP_CFG` — `A5 40 05 E8 03 00 00 00 DE CD`
  - [ ] `PWM_CFG` — `A5 41 03 00 00 05 4C 61`
  - [ ] `PWM_SET` — `A5 42 04 FA 00 00 05 05 C1`
  - [ ] `PWM_RELEASE` — `A5 43 02 00 05 45 4F`
  - [ ] `PWM_GET` — `A5 44 02 00 05 68 1E`
  - [ ] `PWM_GROUP_GET` — `A5 45 01 00 F0 09`
  - [ ] `PWM_GROUP_RELEASE` — `A5 46 01 00 A0 50`
- [ ] Returns the frame length, which is `5 + LEN`.
- [ ] A zero-length payload is legal: LEN is `0` and the frame is 5 bytes.
- [ ] A payload of exactly `RX_MAX_PAYLOAD` (32) is accepted.
- [ ] A payload above `RX_MAX_PAYLOAD` is rejected and writes nothing.
- [ ] An output buffer too small for the frame is rejected and writes nothing.
- [ ] A NULL output buffer is rejected.
- [ ] A NULL payload with a non-zero length is rejected.
- [ ] A payload containing `0xA5` is framed normally — there is no escaping in
      this protocol, and SOF is only a resync anchor.

## parse_response

- [ ] Bare ACK `A5 01 01 1F 3E` decodes as success with no data.
- [ ] `GPIO_READ` ACK `A5 02 01 01 EC 81` yields the one-byte value `0x01`.
- [ ] `PWM_GET` ACK `A5 03 01 FA 00 26 D4` yields `250` — little-endian uint16.
- [ ] `PWM_GROUP_GET` ACK `A5 05 01 E8 03 00 00 39 BF` yields `1000` —
      little-endian uint32.
- [ ] NACK `A5 02 00 06 3A C2` decodes as failure with reason `0x06` (`ERR_BUSY`).
- [ ] The other doc NACKs decode to their reasons: `04` INVALID_STATE,
      `05` NOT_INIT, `02` INVALID_ARG.
- [ ] An unknown reason byte is passed through rather than rejected — the
      firmware's status codes are append-only, so a newer MCU can send one this
      build has never heard of.
- [ ] A NACK is never read as data: check ACK/NACK before the value, since
      `LEN = 0x02` is a valid width for both a `GPIO_READ` refusal and a reason.
- [ ] Wrong SOF is rejected.
- [ ] `LEN = 0` is rejected — the minimum is 1, the ACK/NACK byte itself.
- [ ] `LEN` above `1 + TX_DATA_MAX` (5) is rejected.
- [ ] A truncated buffer — fewer bytes than `4 + LEN` — is rejected.
- [ ] A single-bit flip in any covered byte is rejected on CRC.
- [ ] A flip in the CRC bytes themselves is rejected.
- [ ] CRC is computed over `LEN`, `ACK/NACK` and `DATA` only — not SOF, not the
      CRC bytes. A frame whose SOF differs but whose body is identical must
      produce the same CRC.
- [ ] A NULL input buffer is rejected.
- [ ] Extra bytes after a complete frame are ignored, not an error — the caller
      sizes the read from `LEN`.

---

## Explicitly not this module's job

- **Resync to SOF, response timeouts, one-command-in-flight.** That belongs to the
  caller; `parse_response` receives one complete candidate frame and judges it.
- **Knowing which opcode is in flight.** Responses carry no opcode, so DATA width
  is per-opcode knowledge that lives above this layer. `parse_response` sizes
  everything from `LEN` and hands back raw data bytes.
- **Argument validation** (port, pin, duty ranges). That is `mcuco.c`, which
  builds the payload before calling in here.
- **Silence as an outcome.** The MCU answers a CRC or framing error with nothing
  at all, so "no response" is a timeout in the caller and never reaches here.
