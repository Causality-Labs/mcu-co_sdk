#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "mcuco.h"
#include "uart.h"

struct mcuco
{
    int fd;
    int timeout_ms;
};

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

    int fd = uart_open(device_path);
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

    uart_close(mcu->fd);
    free(mcu);
}
