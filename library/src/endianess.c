#include <stddef.h>

#include "endianess.h"

int put_u16_le(uint8_t *destination, uint16_t value)
{
    if (destination == NULL)
    {
        return -1;
    }

    destination[0] = (uint8_t)(value & 0xFFU);
    destination[1] = (uint8_t)((value >> 8) & 0xFFU);

    return 2;
}

int put_u32_le(uint8_t *destination, uint32_t value)
{
    if (destination == NULL)
    {
        return -1;
    }

    destination[0] = (uint8_t)(value & 0xFFU);
    destination[1] = (uint8_t)((value >> 8) & 0xFFU);
    destination[2] = (uint8_t)((value >> 16) & 0xFFU);
    destination[3] = (uint8_t)((value >> 24) & 0xFFU);

    return 4;
}

int get_u16_le(const uint8_t *source, uint16_t *value)
{
    if (source == NULL || value == NULL)
    {
        return -1;
    }

    *value = (uint16_t)((uint16_t)source[0] | ((uint16_t)source[1] << 8));

    return 2;
}

int get_u32_le(const uint8_t *source, uint32_t *value)
{
    if (source == NULL || value == NULL)
    {
        return -1;
    }

    *value = (uint32_t)source[0] | ((uint32_t)source[1] << 8) | ((uint32_t)source[2] << 16) |
             ((uint32_t)source[3] << 24);

    return 4;
}
