#ifndef ENDIANESS_H
#define ENDIANESS_H

#include <stdint.h>

/* The wire is little-endian regardless of what the host is, so these shift
 * explicitly rather than copying the native representation.
 *
 * Each returns the number of bytes worked with, or -1 for a NULL pointer. */

int put_u16_le(uint8_t *destination, uint16_t value);
int put_u32_le(uint8_t *destination, uint32_t value);

int get_u16_le(const uint8_t *source, uint16_t *value);
int get_u32_le(const uint8_t *source, uint32_t *value);

#endif /* ENDIANESS_H */
