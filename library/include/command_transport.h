#ifndef COMMAND_TRANSPORT_H
#define COMMAND_TRANSPORT_H

#include "protocol.h"
#include "status.h"

/* Opens the command port. Returns the fd, or a negative errno with nothing
 * left open. */
int open_port(const char *device_path);

/* Closes a port from open_port. A negative fd is ignored. */
void close_port(int fd);

/* Writes a built frame to an open port. */
mcu_status_t transmit_command(int fd, const uint8_t *frame, size_t frame_len);

/* Reads one response and decodes it. timeout_ms bounds the whole exchange,
 * not each read: a response arrives in several reads, and the length is not
 * known until LEN has been read. */
mcu_status_t receive_response(int fd, int timeout_ms, protocol_response_t *response);

#endif /* COMMAND_TRANSPORT_H */
