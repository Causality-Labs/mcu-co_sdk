#ifndef PROTOCOL_DEFS_H
#define PROTOCOL_DEFS_H

/* Wire constants from mcu-co_Protocol.md.
 *
 *   command   SOF · OPCODE · LEN · PAYLOAD · CRC_L · CRC_H
 *   response  SOF · LEN · ACK/NACK [ · DATA ] · CRC_L · CRC_H
 *
 * Sizes match the firmware's frame_parser.h but are named after the frame:
 * its RX and TX are relative to the MCU, so those names would invert here.
 * Field encodings live in mcuco.h, where callers can reach them. */

#define PROTOCOL_SOF 0xA5U

#define PROTOCOL_MAX_PAYLOAD       32U /* RX_MAX_PAYLOAD */
#define PROTOCOL_MAX_RESPONSE_DATA 4U  /* TX_DATA_MAX, a uint32 frequency */

#define PROTOCOL_COMMAND_OVERHEAD  5U /* SOF, OPCODE, LEN, CRC_L, CRC_H */
#define PROTOCOL_RESPONSE_OVERHEAD 4U /* SOF, LEN, CRC_L, CRC_H */

#define PROTOCOL_MAX_COMMAND_FRAME (PROTOCOL_COMMAND_OVERHEAD + PROTOCOL_MAX_PAYLOAD)

/* LEN counts the ACK/NACK byte, so it runs from 1 (bare ACK) upward. */
#define PROTOCOL_MIN_RESPONSE_LEN 1U
#define PROTOCOL_MAX_RESPONSE_LEN (PROTOCOL_MIN_RESPONSE_LEN + PROTOCOL_MAX_RESPONSE_DATA)

#define PROTOCOL_MIN_RESPONSE_FRAME (PROTOCOL_RESPONSE_OVERHEAD + PROTOCOL_MIN_RESPONSE_LEN)
#define PROTOCOL_MAX_RESPONSE_FRAME (PROTOCOL_RESPONSE_OVERHEAD + PROTOCOL_MAX_RESPONSE_LEN)

/* Byte positions within a frame. The trailing CRC is at frame_len - 2, which
 * varies with the payload, so it has no fixed index. */
#define COMMAND_SOF_IDX     0U
#define COMMAND_OPCODE_IDX  1U
#define COMMAND_LEN_IDX     2U
#define COMMAND_PAYLOAD_IDX 3U

#define RESPONSE_SOF_IDX  0U
#define RESPONSE_LEN_IDX  1U
#define RESPONSE_ACK_IDX  2U
#define RESPONSE_DATA_IDX 3U

/* Fixed on the wire - never renumber. Thirteen opcodes cover fourteen CLI
 * commands: `gpio irq cfg off` is GPIO_IRQ_CFG with an edge of 0. */
typedef enum
{
    OPCODE_GPIO_CFG        = 0x30,
    OPCODE_GPIO_WRITE      = 0x31,
    OPCODE_GPIO_READ       = 0x32,
    OPCODE_GPIO_IRQ_BIND   = 0x33,
    OPCODE_GPIO_IRQ_CFG    = 0x34,
    OPCODE_GPIO_IRQ_UNBIND = 0x35,

    OPCODE_PWM_GROUP_CFG     = 0x40,
    OPCODE_PWM_CFG           = 0x41,
    OPCODE_PWM_SET           = 0x42,
    OPCODE_PWM_RELEASE       = 0x43,
    OPCODE_PWM_GET           = 0x44,
    OPCODE_PWM_GROUP_GET     = 0x45,
    OPCODE_PWM_GROUP_RELEASE = 0x46,
} protocol_opcode_t;

#endif /* PROTOCOL_DEFS_H */
