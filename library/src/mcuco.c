#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "command_transport.h"
#include "mcuco.h"
#include "protocol.h"

struct mcuco
{
    int fd;
    int timeout_ms;
};

static uint32_t get_u32_le(const uint8_t *source)
{
    return (uint32_t)source[0] | ((uint32_t)source[1] << 8) | ((uint32_t)source[2] << 16) | ((uint32_t)source[3] << 24);
}

/* A NACK carries the firmware's own status code, so it becomes the return
 * value untranslated. */
static mcu_status_t status_from_response(const protocol_response_t *response)
{
    if (response->ack)
    {
        return STATUS_OK;
    }

    if (response->data_len != 1)
    {
        return STATUS_ERR_BAD_FRAME;
    }

    return (mcu_status_t)response->data[0];
}

/* Sends an already-built frame and decodes the reply. `response` may be NULL
 * for commands that return no data. */
static uint16_t get_u16_le(const uint8_t *source)
{
    return (uint16_t)((uint16_t)source[0] | ((uint16_t)source[1] << 8));
}

static mcu_status_t exchange(mcuco_t *mcu, const uint8_t *frame, ssize_t frame_len, protocol_response_t *response)
{
    if (frame_len < 0)
    {
        return (mcu_status_t)(-frame_len);
    }

    protocol_response_t decoded = {0};

    mcu_status_t status = transmit_command(mcu->fd, frame, (size_t)frame_len);
    if (status != STATUS_OK)
    {
        return status;
    }

    status = receive_response(mcu->fd, mcu->timeout_ms, &decoded);
    if (status != STATUS_OK)
    {
        return status;
    }

    status = status_from_response(&decoded);
    if (status != STATUS_OK)
    {
        return status;
    }

    if (response != NULL)
    {
        *response = decoded;
    }

    return STATUS_OK;
}

mcuco_t *mcuco_open(const char *device_path, int timeout_ms)
{
    if (device_path == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    mcuco_t *mcu = malloc(sizeof(struct mcuco));
    if (mcu == NULL)
    {
        return NULL;
    }

    memset(mcu, 0, sizeof(struct mcuco));

    int fd = open_port(device_path);
    if (fd < 0)
    {
        /* free() is not required to preserve errno. */
        int reason = -fd;
        free(mcu);
        errno = reason;
        return NULL;
    }

    mcu->fd         = fd;
    mcu->timeout_ms = timeout_ms;

    return mcu;
}

void mcuco_close(mcuco_t *mcu)
{
    if (mcu == NULL)
    {
        return;
    }

    close_port(mcu->fd);
    free(mcu);
}

mcu_status_t mcuco_gpio_cfg(mcuco_t *mcu, dir_t dir, port_t port, uint8_t pin)
{
    if (mcu == NULL)
    {
        return STATUS_ERR_ARG;
    }

    uint8_t frame[PROTOCOL_MAX_COMMAND_FRAME];
    ssize_t frame_len = protocol_gpio_cfg(dir, port, pin, frame, sizeof(frame));

    return exchange(mcu, frame, frame_len, NULL);
}

mcu_status_t mcuco_gpio_set(mcuco_t *mcu, level_t level, port_t port, uint8_t pin)
{
    if (mcu == NULL)
    {
        return STATUS_ERR_ARG;
    }

    uint8_t frame[PROTOCOL_MAX_COMMAND_FRAME];
    ssize_t frame_len = protocol_gpio_set(level, port, pin, frame, sizeof(frame));

    return exchange(mcu, frame, frame_len, NULL);
}

mcu_status_t mcuco_gpio_get(mcuco_t *mcu, port_t port, uint8_t pin, level_t *level)
{
    if (mcu == NULL || level == NULL)
    {
        return STATUS_ERR_ARG;
    }

    uint8_t frame[PROTOCOL_MAX_COMMAND_FRAME];
    ssize_t frame_len = protocol_gpio_get(port, pin, frame, sizeof(frame));

    protocol_response_t response = {0};

    mcu_status_t status = exchange(mcu, frame, frame_len, &response);
    if (status != STATUS_OK)
    {
        return status;
    }

    if (response.data_len != 1)
    {
        return STATUS_ERR_BAD_FRAME;
    }

    *level = (response.data[0] != 0) ? LEVEL_HIGH : LEVEL_LOW;

    return STATUS_OK;
}

mcu_status_t mcuco_gpio_irq_cfg(mcuco_t *mcu, edge_t edge, port_t port, uint8_t pin)
{
    if (mcu == NULL)
    {
        return STATUS_ERR_ARG;
    }

    uint8_t frame[PROTOCOL_MAX_COMMAND_FRAME];
    ssize_t frame_len = protocol_gpio_irq_cfg(edge, port, pin, frame, sizeof(frame));

    return exchange(mcu, frame, frame_len, NULL);
}

mcu_status_t mcuco_gpio_irq_bind(mcuco_t *mcu, edge_t edge, port_t in_port, uint8_t in_pin, action_t action, port_t out_port,
                                 uint8_t out_pin)
{
    if (mcu == NULL)
    {
        return STATUS_ERR_ARG;
    }

    uint8_t frame[PROTOCOL_MAX_COMMAND_FRAME];
    ssize_t frame_len = protocol_gpio_irq_bind(edge, in_port, in_pin, action, out_port, out_pin, frame, sizeof(frame));

    return exchange(mcu, frame, frame_len, NULL);
}

mcu_status_t mcuco_gpio_irq_unbind(mcuco_t *mcu, port_t port, uint8_t pin)
{
    if (mcu == NULL)
    {
        return STATUS_ERR_ARG;
    }

    uint8_t frame[PROTOCOL_MAX_COMMAND_FRAME];
    ssize_t frame_len = protocol_gpio_irq_unbind(port, pin, frame, sizeof(frame));

    return exchange(mcu, frame, frame_len, NULL);
}

mcu_status_t mcuco_pwm_group_cfg(mcuco_t *mcu, uint32_t freq_hz, uint8_t group)
{
    if (mcu == NULL)
    {
        return STATUS_ERR_ARG;
    }

    uint8_t frame[PROTOCOL_MAX_COMMAND_FRAME];
    ssize_t frame_len = protocol_pwm_group_cfg(freq_hz, group, frame, sizeof(frame));

    return exchange(mcu, frame, frame_len, NULL);
}

mcu_status_t mcuco_pwm_group_get(mcuco_t *mcu, uint8_t group, uint32_t *achieved_hz)
{
    if (mcu == NULL || achieved_hz == NULL)
    {
        return STATUS_ERR_ARG;
    }

    uint8_t frame[PROTOCOL_MAX_COMMAND_FRAME];
    ssize_t frame_len = protocol_pwm_group_get(group, frame, sizeof(frame));

    protocol_response_t response = {0};

    mcu_status_t status = exchange(mcu, frame, frame_len, &response);
    if (status != STATUS_OK)
    {
        return status;
    }

    if (response.data_len != 4)
    {
        return STATUS_ERR_BAD_FRAME;
    }

    *achieved_hz = get_u32_le(response.data);

    return STATUS_OK;
}

mcu_status_t mcuco_pwm_group_release(mcuco_t *mcu, uint8_t group)
{
    if (mcu == NULL)
    {
        return STATUS_ERR_ARG;
    }

    uint8_t frame[PROTOCOL_MAX_COMMAND_FRAME];
    ssize_t frame_len = protocol_pwm_group_release(group, frame, sizeof(frame));

    return exchange(mcu, frame, frame_len, NULL);
}

mcu_status_t mcuco_pwm_channel_cfg(mcuco_t *mcu, polarity_t polarity, port_t port, uint8_t pin)
{
    if (mcu == NULL)
    {
        return STATUS_ERR_ARG;
    }

    uint8_t frame[PROTOCOL_MAX_COMMAND_FRAME];
    ssize_t frame_len = protocol_pwm_channel_cfg(polarity, port, pin, frame, sizeof(frame));

    return exchange(mcu, frame, frame_len, NULL);
}

mcu_status_t mcuco_pwm_channel_set(mcuco_t *mcu, uint16_t duty, port_t port, uint8_t pin)
{
    if (mcu == NULL)
    {
        return STATUS_ERR_ARG;
    }

    uint8_t frame[PROTOCOL_MAX_COMMAND_FRAME];
    ssize_t frame_len = protocol_pwm_channel_set(duty, port, pin, frame, sizeof(frame));

    return exchange(mcu, frame, frame_len, NULL);
}

mcu_status_t mcuco_pwm_channel_get(mcuco_t *mcu, port_t port, uint8_t pin, uint16_t *duty)
{
    if (mcu == NULL || duty == NULL)
    {
        return STATUS_ERR_ARG;
    }

    uint8_t frame[PROTOCOL_MAX_COMMAND_FRAME];
    ssize_t frame_len = protocol_pwm_channel_get(port, pin, frame, sizeof(frame));

    protocol_response_t response = {0};

    mcu_status_t status = exchange(mcu, frame, frame_len, &response);
    if (status != STATUS_OK)
    {
        return status;
    }

    if (response.data_len != 2)
    {
        return STATUS_ERR_BAD_FRAME;
    }

    *duty = get_u16_le(response.data);

    return STATUS_OK;
}

mcu_status_t mcuco_pwm_channel_release(mcuco_t *mcu, port_t port, uint8_t pin)
{
    if (mcu == NULL)
    {
        return STATUS_ERR_ARG;
    }

    uint8_t frame[PROTOCOL_MAX_COMMAND_FRAME];
    ssize_t frame_len = protocol_pwm_channel_release(port, pin, frame, sizeof(frame));

    return exchange(mcu, frame, frame_len, NULL);
}
