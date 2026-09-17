#ifndef CRC16_H
#define CRC16_H

#include <stddef.h>
#include <stdint.h>

/* CRC16-CCITT-FALSE: poly 0x1021, init 0xFFFF, unreflected, no final XOR.
 * Same algorithm as the firmware's crc16_compute(). */
uint16_t crc16_compute(const uint8_t *data, size_t len);

#endif /* CRC16_H */
