#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#include "mcuco.h"
#include "protocol_defs.h"
#include "status.h"

/* `ack` disambiguates `data`: on ACK it is a read's value, on NACK a reason
 * byte. Kept raw because LEN, not the opcode, sizes the frame - so decoding
 * needs no knowledge of which command was sent. */
typedef struct
{
    bool    ack;
    uint8_t data[PROTOCOL_MAX_RESPONSE_DATA];
    size_t  data_len;
} protocol_response_t;

/* One builder per command. Each writes the complete frame - SOF, opcode,
 * length, payload and CRC - that mcu-co_Protocol.md specifies, and rejects
 * out-of-range arguments, so no caller needs to know either.
 *
 * `buffer` must hold at least PROTOCOL_MAX_COMMAND_FRAME bytes. Returns the
 * number of bytes written, or a negative mcu_status_t. */
ssize_t protocol_gpio_cfg(dir_t dir, port_t port, uint8_t pin, uint8_t *buffer,
                          size_t buffer_len);

/* Decodes one complete candidate frame. Verifies SOF, LEN and the CRC before
 * filling `response`; a frame that fails any of those is STATUS_ERR_BAD_FRAME.
 * Bytes past the frame are ignored. */
mcu_status_t protocol_parse_response(const uint8_t *frame, size_t frame_len,
                                     protocol_response_t *response);

#endif /* PROTOCOL_H */
