#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "protocol_defs.h"
#include "status.h"

/* Payload is by value, not pointer and length: a caller cannot then hand over
 * a short buffer while claiming a longer one. Bytes past payload_len are
 * ignored. */
typedef struct
{
    protocol_opcode_t opcode;
    uint8_t payload[PROTOCOL_MAX_PAYLOAD];
    size_t  payload_len;
} protocol_command_t;

/* `ack` disambiguates `data`: on ACK it is a read's value, on NACK a reason
 * byte. Kept raw because LEN, not the opcode, sizes the frame - so decoding
 * needs no knowledge of which command was sent. */
typedef struct
{
    bool    ack;
    uint8_t data[PROTOCOL_MAX_RESPONSE_DATA];
    size_t  data_len;
} protocol_response_t;

#endif /* PROTOCOL_H */
