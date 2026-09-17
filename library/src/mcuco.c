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

/* A NACK carries the firmware's own status code, so it becomes the return
 * value untranslated. */
static mcu_status_t status_from_response(const protocol_response_t *response)
{
    if (response->ack)
        return STATUS_OK;

    if (response->data_len != 1)
        return STATUS_ERR_BAD_FRAME;

    return (mcu_status_t)response->data[0];
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
        return NULL;

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
        return;

    close_port(mcu->fd);
    free(mcu);
}

mcu_status_t mcuco_gpio_cfg(mcuco_t *mcu, dir_t dir, port_t port, uint8_t pin)
{
    if (mcu == NULL)
        return STATUS_ERR_ARG;

    uint8_t frame[PROTOCOL_MAX_COMMAND_FRAME];

    ssize_t frame_len = protocol_gpio_cfg(dir, port, pin, frame, sizeof(frame));
    if (frame_len < 0)
        return (mcu_status_t)(-frame_len);

    protocol_response_t response = {0};

    mcu_status_t status = transmit_command(mcu->fd, frame, (size_t)frame_len);
    if (status != STATUS_OK)
        return status;

    status = receive_response(mcu->fd, mcu->timeout_ms, &response);
    if (status != STATUS_OK)
        return status;

    return status_from_response(&response);
}
