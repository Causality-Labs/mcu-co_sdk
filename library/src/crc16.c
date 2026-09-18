#include "crc16.h"

#define CRC16_POLYNOMIAL 0x1021U
#define CRC16_SEED       0xFFFFU

uint16_t crc16_compute(const uint8_t *data, size_t len)
{
    uint16_t crc = CRC16_SEED;

    if (data == NULL)
    {
        return crc;
    }

    for (size_t i = 0; i < len; i++)
    {
        crc ^= (uint16_t)((uint16_t)data[i] << 8);

        for (int bit = 0; bit < 8; bit++)
        {
            if ((crc & 0x8000U) != 0U)
            {
                crc = (uint16_t)((uint16_t)(crc << 1) ^ CRC16_POLYNOMIAL);
            }
            else
            {
                crc = (uint16_t)(crc << 1);
            }
        }
    }

    return crc;
}
