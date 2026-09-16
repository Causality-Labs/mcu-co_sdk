#define _DEFAULT_SOURCE /* CRTSCTS, cfmakeraw */

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "uart.h"

/* The MCU is fixed at this rate; there is nothing to negotiate. */
#define MCU_BAUD B115200

static int configure_port(int fd)
{
    struct termios tty;
    if (tcgetattr(fd, &tty) != 0)
        return -errno;

    cfmakeraw(&tty);

    tty.c_cflag |= (tcflag_t)(CLOCAL | CREAD);
    tty.c_cflag &= (tcflag_t)~(PARENB | CSTOPB | CRTSCTS);
    tty.c_cflag &= (tcflag_t)~CSIZE;
    tty.c_cflag |= (tcflag_t)CS8;

    /* VTIME is per-read, so a deadline set here would multiply by the frame
     * length. The caller polls instead. */
    tty.c_cc[VMIN]  = 0;
    tty.c_cc[VTIME] = 0;

    if (cfsetispeed(&tty, MCU_BAUD) != 0)
        return -errno;
    if (cfsetospeed(&tty, MCU_BAUD) != 0)
        return -errno;

    if (tcsetattr(fd, TCSANOW, &tty) != 0)
        return -errno;

    /* tcsetattr succeeds if it applied ANY change, not all of them. */
    struct termios actual;
    if (tcgetattr(fd, &actual) != 0)
        return -errno;

    if ((actual.c_cflag & CSIZE) != CS8)
        return -EINVAL;
    if ((actual.c_cflag & (PARENB | CSTOPB)) != 0)
        return -EINVAL;
    if (cfgetospeed(&actual) != MCU_BAUD)
        return -EINVAL;

    tcflush(fd, TCIFLUSH);

    if (fcntl(fd, F_SETFL, 0) != 0)
        return -errno;

    return 0;
}

static long now_ms(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    long milliseconds = (now.tv_sec * 1000L) + (now.tv_nsec / 1000000L);

    return milliseconds;
}

int uart_open(const char *device_path)
{
    /* O_NOCTTY: a signal on an adopted controlling terminal would kill us.
     * O_NONBLOCK: skip the carrier wait; cleared once CLOCAL is set. */
    int fd = open(device_path, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0)
        return -errno;

    int status = configure_port(fd);
    if (status != 0)
    {
        close(fd);
        return status;
    }

    return fd;
}

void uart_close(int fd)
{
    if (fd < 0)
        return;

    close(fd);
}

ssize_t uart_read(int fd, uint8_t *buf, size_t len, int timeout_ms)
{
    long deadline_ms = now_ms() + timeout_ms;
    size_t total_read = 0;

    while (total_read < len)
    {
        long time_left_ms = deadline_ms - now_ms();

        if (time_left_ms <= 0)
            return -ETIMEDOUT;

        struct pollfd poll_fd = {.fd = fd, .events = POLLIN, .revents = 0};

        int ready = poll(&poll_fd, 1, (int)time_left_ms);

        if (ready < 0)
        {
            if (errno == EINTR)
                continue;
            return -errno;
        }

        if (ready == 0)
            return -ETIMEDOUT;

        /* POLLIN was the only request, so anything else is HUP or ERR. */
        if ((poll_fd.revents & POLLIN) == 0)
            return -ENOTCONN;

        ssize_t bytes_read = read(fd, buf + total_read, len - total_read);

        if (bytes_read < 0)
        {
            if (errno == EINTR || errno == EAGAIN)
                continue;
            return -errno;
        }

        if (bytes_read == 0)
            return -ENOTCONN;

        total_read += (size_t)bytes_read;
    }

    return (ssize_t)total_read;
}

ssize_t uart_write(int fd, const uint8_t *buf, size_t len)
{
    size_t total_written = 0;
    while (total_written < len)
    {
        ssize_t bytes_written = write(fd, buf + total_written, len - total_written);
        if (bytes_written < 0)
        {
            if (errno == EINTR)
                continue;
            return -errno;
        }

        total_written += (size_t)bytes_written;
    }

    return (ssize_t)total_written;
}
