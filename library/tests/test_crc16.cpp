#include "CppUTest/TestHarness.h"

extern "C" {
#include "crc16.h"
}

TEST_GROUP(Crc16){};

/* --- crc16_compute --- */

// Section 1 of mcu-co_Protocol.md frames "gpio cfg output A 5" as
// A5 30 03 01 00 05 AB E1. The CRC covers OPCODE, LEN and PAYLOAD - not SOF,
// not the CRC bytes - and is sent little-endian, so AB E1 is 0xE1AB.
TEST(Crc16, MatchesTheWorkedFrameInTheProtocolDoc)
{
    const uint8_t covered[] = {0x30, 0x03, 0x01, 0x00, 0x05};

    LONGS_EQUAL(0xE1AB, crc16_compute(covered, sizeof(covered)));
}
