#include <stddef.h>

#include "crc16.h"
#include "protocol.h"

static bool is_valid_port(port_t port)
{
    return port <= PORT_G;
}

static bool is_valid_pin(uint8_t pin)
{
    return pin <= PIN_MAX;
}

/* SOF · OPCODE · LEN · PAYLOAD · CRC_L · CRC_H, with the CRC over everything
 * but SOF and the CRC bytes themselves, little-endian on the wire. */
static ssize_t build_frame(protocol_opcode_t opcode, const uint8_t *payload,
                           size_t payload_len, uint8_t *buffer, size_t buffer_len)
{
    if (buffer == NULL || payload_len > PROTOCOL_MAX_PAYLOAD)
        return -STATUS_ERR_ARG;

    size_t frame_len = PROTOCOL_COMMAND_OVERHEAD + payload_len;
    if (buffer_len < frame_len)
        return -STATUS_ERR_ARG;

    buffer[COMMAND_SOF_IDX]    = PROTOCOL_SOF;
    buffer[COMMAND_OPCODE_IDX] = (uint8_t)opcode;
    buffer[COMMAND_LEN_IDX]    = (uint8_t)payload_len;

    for (size_t i = 0; i < payload_len; i++)
        buffer[COMMAND_PAYLOAD_IDX + i] = payload[i];

    /* The CRC covers OPCODE, LEN and PAYLOAD - not SOF, not itself. */
    uint16_t crc = crc16_compute(&buffer[COMMAND_OPCODE_IDX], payload_len + 2);

    buffer[frame_len - 2] = (uint8_t)(crc & 0xFFU);
    buffer[frame_len - 1] = (uint8_t)(crc >> 8);

    return (ssize_t)frame_len;
}

ssize_t protocol_gpio_cfg(dir_t dir, port_t port, uint8_t pin, uint8_t *buffer,
                          size_t buffer_len)
{
    if (dir != DIR_INPUT && dir != DIR_OUTPUT)
        return -STATUS_ERR_ARG;

    if (!is_valid_port(port) || !is_valid_pin(pin))
        return -STATUS_ERR_ARG;

    uint8_t payload[3];
    payload[0] = (uint8_t)dir;
    payload[1] = (uint8_t)port;
    payload[2] = pin;

    return build_frame(OPCODE_GPIO_CFG, payload, sizeof(payload), buffer, buffer_len);
}

mcu_status_t protocol_parse_response(const uint8_t *frame, size_t frame_len,
                                     protocol_response_t *response)
{
    if (frame == NULL || response == NULL)
        return STATUS_ERR_ARG;

    if (frame_len < PROTOCOL_MIN_RESPONSE_FRAME || frame[RESPONSE_SOF_IDX] != PROTOCOL_SOF)
        return STATUS_ERR_BAD_FRAME;

    size_t length = frame[RESPONSE_LEN_IDX];
    if (length < PROTOCOL_MIN_RESPONSE_LEN || length > PROTOCOL_MAX_RESPONSE_LEN)
        return STATUS_ERR_BAD_FRAME;

    if (frame_len < PROTOCOL_RESPONSE_OVERHEAD + length)
        return STATUS_ERR_BAD_FRAME;

    /* The CRC covers LEN, ACK/NACK and DATA - not SOF, not itself. */
    uint16_t computed = crc16_compute(&frame[RESPONSE_LEN_IDX], length + 1);
    uint16_t received =
        (uint16_t)(frame[RESPONSE_ACK_IDX + length] | (frame[RESPONSE_DATA_IDX + length] << 8));

    if (computed != received)
        return STATUS_ERR_BAD_FRAME;

    response->ack      = (frame[RESPONSE_ACK_IDX] != 0);
    response->data_len = length - 1;

    for (size_t i = 0; i < response->data_len; i++)
        response->data[i] = frame[RESPONSE_DATA_IDX + i];

    return STATUS_OK;
}
