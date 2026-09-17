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

static bool is_valid_group(uint8_t group)
{
    return group <= GROUP_MAX;
}

static void put_u32_le(uint8_t *destination, uint32_t value)
{
    destination[0] = (uint8_t)(value & 0xFFU);
    destination[1] = (uint8_t)((value >> 8) & 0xFFU);
    destination[2] = (uint8_t)((value >> 16) & 0xFFU);
    destination[3] = (uint8_t)((value >> 24) & 0xFFU);
}

static void put_u16_le(uint8_t *destination, uint16_t value)
{
    destination[0] = (uint8_t)(value & 0xFFU);
    destination[1] = (uint8_t)((value >> 8) & 0xFFU);
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

ssize_t protocol_gpio_set(level_t level, port_t port, uint8_t pin, uint8_t *buffer,
                          size_t buffer_len)
{
    if (level != LEVEL_LOW && level != LEVEL_HIGH)
        return -STATUS_ERR_ARG;

    if (!is_valid_port(port) || !is_valid_pin(pin))
        return -STATUS_ERR_ARG;

    uint8_t payload[3];
    payload[0] = (uint8_t)level;
    payload[1] = (uint8_t)port;
    payload[2] = pin;

    return build_frame(OPCODE_GPIO_WRITE, payload, sizeof(payload), buffer, buffer_len);
}

ssize_t protocol_gpio_get(port_t port, uint8_t pin, uint8_t *buffer, size_t buffer_len)
{
    if (!is_valid_port(port) || !is_valid_pin(pin))
        return -STATUS_ERR_ARG;

    uint8_t payload[2];
    payload[0] = (uint8_t)port;
    payload[1] = pin;

    return build_frame(OPCODE_GPIO_READ, payload, sizeof(payload), buffer, buffer_len);
}

ssize_t protocol_pwm_group_cfg(uint32_t freq_hz, uint8_t group, uint8_t *buffer,
                               size_t buffer_len)
{
    if (freq_hz < FREQ_MIN || freq_hz > FREQ_MAX)
        return -STATUS_ERR_ARG;

    if (!is_valid_group(group))
        return -STATUS_ERR_ARG;

    uint8_t payload[5];
    put_u32_le(payload, freq_hz);
    payload[4] = group;

    return build_frame(OPCODE_PWM_GROUP_CFG, payload, sizeof(payload), buffer, buffer_len);
}

ssize_t protocol_pwm_group_get(uint8_t group, uint8_t *buffer, size_t buffer_len)
{
    if (!is_valid_group(group))
        return -STATUS_ERR_ARG;

    uint8_t payload[1];
    payload[0] = group;

    return build_frame(OPCODE_PWM_GROUP_GET, payload, sizeof(payload), buffer, buffer_len);
}

ssize_t protocol_gpio_irq_cfg(edge_t edge, port_t port, uint8_t pin, uint8_t *buffer,
                              size_t buffer_len)
{
    if (edge > EDGE_BOTH)
        return -STATUS_ERR_ARG;

    if (!is_valid_port(port) || !is_valid_pin(pin))
        return -STATUS_ERR_ARG;

    uint8_t payload[3];
    payload[0] = (uint8_t)edge;
    payload[1] = (uint8_t)port;
    payload[2] = pin;

    return build_frame(OPCODE_GPIO_IRQ_CFG, payload, sizeof(payload), buffer, buffer_len);
}

ssize_t protocol_gpio_irq_bind(edge_t edge, port_t in_port, uint8_t in_pin,
                               action_t action, port_t out_port, uint8_t out_pin,
                               uint8_t *buffer, size_t buffer_len)
{
    /* Dropping a binding is gpio irq unbind; disarming is gpio irq cfg off. */
    if (edge < EDGE_RISING || edge > EDGE_BOTH)
        return -STATUS_ERR_ARG;

    if (action > ACTION_TOGGLE)
        return -STATUS_ERR_ARG;

    if (!is_valid_port(in_port) || !is_valid_pin(in_pin))
        return -STATUS_ERR_ARG;

    if (!is_valid_port(out_port) || !is_valid_pin(out_pin))
        return -STATUS_ERR_ARG;

    uint8_t payload[6];
    payload[0] = (uint8_t)edge;
    payload[1] = (uint8_t)in_port;
    payload[2] = in_pin;
    payload[3] = (uint8_t)action;
    payload[4] = (uint8_t)out_port;
    payload[5] = out_pin;

    return build_frame(OPCODE_GPIO_IRQ_BIND, payload, sizeof(payload), buffer, buffer_len);
}

ssize_t protocol_gpio_irq_unbind(port_t port, uint8_t pin, uint8_t *buffer,
                                 size_t buffer_len)
{
    if (!is_valid_port(port) || !is_valid_pin(pin))
        return -STATUS_ERR_ARG;

    uint8_t payload[2];
    payload[0] = (uint8_t)port;
    payload[1] = pin;

    return build_frame(OPCODE_GPIO_IRQ_UNBIND, payload, sizeof(payload), buffer, buffer_len);
}

ssize_t protocol_pwm_group_release(uint8_t group, uint8_t *buffer, size_t buffer_len)
{
    if (!is_valid_group(group))
        return -STATUS_ERR_ARG;

    uint8_t payload[1];
    payload[0] = group;

    return build_frame(OPCODE_PWM_GROUP_RELEASE, payload, sizeof(payload), buffer,
                       buffer_len);
}

ssize_t protocol_pwm_channel_cfg(polarity_t polarity, port_t port, uint8_t pin,
                                 uint8_t *buffer, size_t buffer_len)
{
    if (polarity != POL_ACTIVE_HIGH && polarity != POL_ACTIVE_LOW)
        return -STATUS_ERR_ARG;

    if (!is_valid_port(port) || !is_valid_pin(pin))
        return -STATUS_ERR_ARG;

    uint8_t payload[3];
    payload[0] = (uint8_t)polarity;
    payload[1] = (uint8_t)port;
    payload[2] = pin;

    return build_frame(OPCODE_PWM_CFG, payload, sizeof(payload), buffer, buffer_len);
}

ssize_t protocol_pwm_channel_set(uint16_t duty, port_t port, uint8_t pin,
                                 uint8_t *buffer, size_t buffer_len)
{
    if (duty > DUTY_MAX)
        return -STATUS_ERR_ARG;

    if (!is_valid_port(port) || !is_valid_pin(pin))
        return -STATUS_ERR_ARG;

    uint8_t payload[4];
    put_u16_le(payload, duty);
    payload[2] = (uint8_t)port;
    payload[3] = pin;

    return build_frame(OPCODE_PWM_SET, payload, sizeof(payload), buffer, buffer_len);
}

ssize_t protocol_pwm_channel_get(port_t port, uint8_t pin, uint8_t *buffer,
                                 size_t buffer_len)
{
    if (!is_valid_port(port) || !is_valid_pin(pin))
        return -STATUS_ERR_ARG;

    uint8_t payload[2];
    payload[0] = (uint8_t)port;
    payload[1] = pin;

    return build_frame(OPCODE_PWM_GET, payload, sizeof(payload), buffer, buffer_len);
}

ssize_t protocol_pwm_channel_release(port_t port, uint8_t pin, uint8_t *buffer,
                                     size_t buffer_len)
{
    if (!is_valid_port(port) || !is_valid_pin(pin))
        return -STATUS_ERR_ARG;

    uint8_t payload[2];
    payload[0] = (uint8_t)port;
    payload[1] = pin;

    return build_frame(OPCODE_PWM_RELEASE, payload, sizeof(payload), buffer, buffer_len);
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
