#include "CppUTest/TestHarness.h"

extern "C" {
#include "protocol.h"
}

TEST_GROUP(Protocol){};

/* --- protocol_gpio_cfg --- */

// Section 1 of mcu-co_Protocol.md: "gpio cfg output A 5".
TEST(Protocol, GpioCfgMatchesTheWorkedFrameInTheProtocolDoc)
{
    const uint8_t expected[] = {0xA5, 0x30, 0x03, 0x01, 0x00, 0x05, 0xAB, 0xE1};
    uint8_t frame[PROTOCOL_MAX_COMMAND_FRAME] = {0};

    LONGS_EQUAL(sizeof(expected),
                protocol_gpio_cfg(DIR_OUTPUT, PORT_A, 5, frame, sizeof(frame)));
    MEMCMP_EQUAL(expected, frame, sizeof(expected));
}

TEST(Protocol, GpioCfgRejectsAnInvalidDirection)
{
    uint8_t frame[PROTOCOL_MAX_COMMAND_FRAME] = {0};

    LONGS_EQUAL(-STATUS_ERR_ARG,
                protocol_gpio_cfg((dir_t)2, PORT_A, 5, frame, sizeof(frame)));
}

TEST(Protocol, GpioCfgRejectsAPinAboveTheMaximum)
{
    uint8_t frame[PROTOCOL_MAX_COMMAND_FRAME] = {0};

    LONGS_EQUAL(-STATUS_ERR_ARG,
                protocol_gpio_cfg(DIR_OUTPUT, PORT_A, PIN_MAX + 1, frame, sizeof(frame)));
}

TEST(Protocol, GpioCfgRejectsABufferTooSmallForTheFrame)
{
    uint8_t frame[7] = {0};

    LONGS_EQUAL(-STATUS_ERR_ARG,
                protocol_gpio_cfg(DIR_OUTPUT, PORT_A, 5, frame, sizeof(frame)));
}

/* --- protocol_parse_response --- */

// Every worked response frame in mcu-co_Protocol.md, by LEN.
TEST(Protocol, ParsesABareAck)
{
    const uint8_t frame[] = {0xA5, 0x01, 0x01, 0x1F, 0x3E};
    protocol_response_t response = {};

    LONGS_EQUAL(STATUS_OK, protocol_parse_response(frame, sizeof(frame), &response));
    CHECK_TRUE(response.ack);
    LONGS_EQUAL(0, response.data_len);
}

TEST(Protocol, ParsesAGpioReadValue)
{
    const uint8_t frame[] = {0xA5, 0x02, 0x01, 0x01, 0xEC, 0x81};
    protocol_response_t response = {};

    LONGS_EQUAL(STATUS_OK, protocol_parse_response(frame, sizeof(frame), &response));
    CHECK_TRUE(response.ack);
    LONGS_EQUAL(1, response.data_len);
    LONGS_EQUAL(0x01, response.data[0]);
}

TEST(Protocol, ParsesAPwmDutyValue)
{
    const uint8_t frame[] = {0xA5, 0x03, 0x01, 0xFA, 0x00, 0x26, 0xD4};
    protocol_response_t response = {};

    LONGS_EQUAL(STATUS_OK, protocol_parse_response(frame, sizeof(frame), &response));
    LONGS_EQUAL(2, response.data_len);
    LONGS_EQUAL(250, response.data[0] | (response.data[1] << 8));
}

TEST(Protocol, ParsesAPwmGroupFrequency)
{
    const uint8_t frame[] = {0xA5, 0x05, 0x01, 0xE8, 0x03, 0x00, 0x00, 0x39, 0xBF};
    protocol_response_t response = {};

    LONGS_EQUAL(STATUS_OK, protocol_parse_response(frame, sizeof(frame), &response));
    LONGS_EQUAL(4, response.data_len);
}

// A NACK carries the firmware's status_t as its single data byte.
TEST(Protocol, ParsesANackAndItsReason)
{
    const uint8_t frame[] = {0xA5, 0x02, 0x00, 0x06, 0x3A, 0xC2};
    protocol_response_t response = {};

    LONGS_EQUAL(STATUS_OK, protocol_parse_response(frame, sizeof(frame), &response));
    CHECK_FALSE(response.ack);
    LONGS_EQUAL(1, response.data_len);
    LONGS_EQUAL(STATUS_ERR_BUSY, response.data[0]);
}

TEST(Protocol, RejectsAFrameWithTheWrongStartByte)
{
    const uint8_t frame[] = {0x5A, 0x01, 0x01, 0x1F, 0x3E};
    protocol_response_t response = {};

    LONGS_EQUAL(STATUS_ERR_BAD_FRAME,
                protocol_parse_response(frame, sizeof(frame), &response));
}

TEST(Protocol, RejectsASingleBitCrcFlip)
{
    uint8_t frame[] = {0xA5, 0x01, 0x01, 0x1F, 0x3E};
    protocol_response_t response = {};

    frame[2] ^= 0x01;

    LONGS_EQUAL(STATUS_ERR_BAD_FRAME,
                protocol_parse_response(frame, sizeof(frame), &response));
}

// LEN counts the ACK/NACK byte, so it is never zero.
TEST(Protocol, RejectsAZeroLength)
{
    const uint8_t frame[] = {0xA5, 0x00, 0x00, 0x00, 0x00};
    protocol_response_t response = {};

    LONGS_EQUAL(STATUS_ERR_BAD_FRAME,
                protocol_parse_response(frame, sizeof(frame), &response));
}

TEST(Protocol, RejectsALengthAboveTheWidestResponse)
{
    const uint8_t frame[] = {0xA5, 0x06, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    protocol_response_t response = {};

    LONGS_EQUAL(STATUS_ERR_BAD_FRAME,
                protocol_parse_response(frame, sizeof(frame), &response));
}

TEST(Protocol, RejectsATruncatedFrame)
{
    const uint8_t frame[] = {0xA5, 0x05, 0x01, 0xE8};
    protocol_response_t response = {};

    LONGS_EQUAL(STATUS_ERR_BAD_FRAME,
                protocol_parse_response(frame, sizeof(frame), &response));
}
