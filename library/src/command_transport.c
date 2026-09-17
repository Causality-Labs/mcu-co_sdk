#include <errno.h>
#include <time.h>

#include "command_transport.h"
#include "uart.h"

static long now_ms(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    long milliseconds = (now.tv_sec * 1000L) + (now.tv_nsec / 1000000L);

    return milliseconds;
}

/* A response arrives in several reads and its length is not known until LEN
 * has been read, so every read shares one deadline rather than getting its
 * own. */
static mcu_status_t read_exactly(int fd, uint8_t *buffer, size_t len, long deadline_ms)
{
    long time_left_ms = deadline_ms - now_ms();
    if (time_left_ms <= 0)
        return STATUS_ERR_NO_RESPONSE;

    ssize_t bytes_read = uart_read(fd, buffer, len, (int)time_left_ms);
    if (bytes_read == -ETIMEDOUT)
        return STATUS_ERR_NO_RESPONSE;
    if (bytes_read < 0)
        return STATUS_ERR_IO;

    return STATUS_OK;
}

int open_port(const char *device_path)
{
    return uart_open(device_path);
}

void close_port(int fd)
{
    uart_close(fd);
}

mcu_status_t transmit_command(int fd, const uint8_t *frame, size_t frame_len)
{
    if (frame == NULL || frame_len == 0)
        return STATUS_ERR_ARG;

    if (uart_write(fd, frame, frame_len) < 0)
        return STATUS_ERR_IO;

    return STATUS_OK;
}

mcu_status_t receive_response(int fd, int timeout_ms, protocol_response_t *response)
{
    if (response == NULL)
        return STATUS_ERR_ARG;

    long deadline_ms = now_ms() + timeout_ms;

    uint8_t frame[PROTOCOL_MAX_RESPONSE_FRAME] = {0};

    /* Discard anything before SOF: a late reply to an abandoned command can
     * still be queued ahead of this one. */
    do
    {
        mcu_status_t status = read_exactly(fd, &frame[RESPONSE_SOF_IDX], 1, deadline_ms);
        if (status != STATUS_OK)
            return status;
    } while (frame[RESPONSE_SOF_IDX] != PROTOCOL_SOF);

    mcu_status_t status = read_exactly(fd, &frame[RESPONSE_LEN_IDX], 1, deadline_ms);
    if (status != STATUS_OK)
        return status;

    size_t length = frame[RESPONSE_LEN_IDX];
    if (length < PROTOCOL_MIN_RESPONSE_LEN || length > PROTOCOL_MAX_RESPONSE_LEN)
        return STATUS_ERR_BAD_FRAME;

    /* LEN counts the ACK/NACK byte and any data; the CRC follows. */
    size_t remaining = length + 2;

    status = read_exactly(fd, &frame[RESPONSE_ACK_IDX], remaining, deadline_ms);
    if (status != STATUS_OK)
        return status;

    return protocol_parse_response(frame, PROTOCOL_RESPONSE_OVERHEAD + length, response);
}
