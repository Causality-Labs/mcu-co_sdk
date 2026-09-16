#ifndef UART_H
#define UART_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

/* Opens device_path 8-bit clean at 115200 8N1, no flow control - the rate the
 * MCU's command_transport_init() is fixed at. Returns the fd, or a negative
 * errno with nothing left open. Reads do not block for a count - poll the fd to
 * put a deadline on a frame. */
int uart_open(const char *device_path);

/* Closes a port from uart_open. A negative fd is ignored, so the failure
 * return of uart_open can be passed straight through. No return value: Linux
 * releases the descriptor even when close() reports an error, so there is
 * nothing a caller could usefully do about one. */
void uart_close(int fd);

/* Reads exactly len bytes, or fails. timeout_ms bounds the whole call, not
 * each read, so a frame arriving one byte at a time still expires on time.
 * Returns len, -ETIMEDOUT if the deadline passes first, or another negative
 * errno. -ENOTCONN means the device went away mid-read. */
ssize_t uart_read(int fd, uint8_t *buf, size_t len, int timeout_ms);

/* Writes exactly len bytes, or fails. Returns len, or a negative errno.
 * Returning means the bytes reached the kernel, not that they left the wire. */
ssize_t uart_write(int fd, const uint8_t *buf, size_t len);

#endif /* UART_H */
