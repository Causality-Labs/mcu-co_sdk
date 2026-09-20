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

/* --- protocol_gpio_set --- */

// Section 2 of mcu-co_Protocol.md: "gpio set high A 5".
TEST(Protocol, GpioSetMatchesTheWorkedFrameInTheProtocolDoc)
{
    const uint8_t expected[] = {0xA5, 0x31, 0x03, 0x01, 0x00, 0x05, 0xFA, 0x4B};
    uint8_t frame[PROTOCOL_MAX_COMMAND_FRAME] = {0};

    LONGS_EQUAL(sizeof(expected),
                protocol_gpio_set(LEVEL_HIGH, PORT_A, 5, frame, sizeof(frame)));
    MEMCMP_EQUAL(expected, frame, sizeof(expected));
}

TEST(Protocol, GpioSetRejectsAnInvalidLevel)
{
    uint8_t frame[PROTOCOL_MAX_COMMAND_FRAME] = {0};

    LONGS_EQUAL(-STATUS_ERR_ARG,
                protocol_gpio_set((level_t)2, PORT_A, 5, frame, sizeof(frame)));
}

TEST(Protocol, GpioSetRejectsAPortAboveTheMaximum)
{
    uint8_t frame[PROTOCOL_MAX_COMMAND_FRAME] = {0};

    LONGS_EQUAL(-STATUS_ERR_ARG,
                protocol_gpio_set(LEVEL_HIGH, (port_t)(PORT_G + 1), 5, frame, sizeof(frame)));
}

/* --- protocol_gpio_get --- */

// Section 3 of mcu-co_Protocol.md: "gpio get A 5". No qualifier byte.
TEST(Protocol, GpioGetMatchesTheWorkedFrameInTheProtocolDoc)
{
    const uint8_t expected[] = {0xA5, 0x32, 0x02, 0x00, 0x05, 0x84, 0x7B};
    uint8_t frame[PROTOCOL_MAX_COMMAND_FRAME] = {0};

    LONGS_EQUAL(sizeof(expected),
                protocol_gpio_get(PORT_A, 5, frame, sizeof(frame)));
    MEMCMP_EQUAL(expected, frame, sizeof(expected));
}

TEST(Protocol, GpioGetRejectsAPinAboveTheMaximum)
{
    uint8_t frame[PROTOCOL_MAX_COMMAND_FRAME] = {0};

    LONGS_EQUAL(-STATUS_ERR_ARG,
                protocol_gpio_get(PORT_A, PIN_MAX + 1, frame, sizeof(frame)));
}

/* --- protocol_pwm_group_cfg --- */

// Section 7 of mcu-co_Protocol.md: "pwm group cfg 1000 0". The frequency is a
// little-endian uint32, so 1000 is E8 03 00 00.
TEST(Protocol, PwmGroupCfgMatchesTheWorkedFrameInTheProtocolDoc)
{
    const uint8_t expected[] = {0xA5, 0x40, 0x05, 0xE8, 0x03, 0x00, 0x00, 0x00, 0xDE, 0xCD};
    uint8_t frame[PROTOCOL_MAX_COMMAND_FRAME] = {0};

    LONGS_EQUAL(sizeof(expected),
                protocol_pwm_group_cfg(1000, 0, frame, sizeof(frame)));
    MEMCMP_EQUAL(expected, frame, sizeof(expected));
}

// Zero is not shorthand for teardown - that is pwm group release.
TEST(Protocol, PwmGroupCfgRejectsAZeroFrequency)
{
    uint8_t frame[PROTOCOL_MAX_COMMAND_FRAME] = {0};

    LONGS_EQUAL(-STATUS_ERR_ARG, protocol_pwm_group_cfg(0, 0, frame, sizeof(frame)));
}

TEST(Protocol, PwmGroupCfgRejectsAFrequencyAboveTheMaximum)
{
    uint8_t frame[PROTOCOL_MAX_COMMAND_FRAME] = {0};

    LONGS_EQUAL(-STATUS_ERR_ARG,
                protocol_pwm_group_cfg(FREQ_MAX + 1, 0, frame, sizeof(frame)));
}

TEST(Protocol, PwmGroupCfgRejectsAGroupAboveTheMaximum)
{
    uint8_t frame[PROTOCOL_MAX_COMMAND_FRAME] = {0};

    LONGS_EQUAL(-STATUS_ERR_ARG,
                protocol_pwm_group_cfg(1000, GROUP_MAX + 1, frame, sizeof(frame)));
}

// The widest frequency must survive the little-endian split intact.
TEST(Protocol, PwmGroupCfgEncodesTheMaximumFrequencyLittleEndian)
{
    uint8_t frame[PROTOCOL_MAX_COMMAND_FRAME] = {0};

    LONGS_EQUAL(10, protocol_pwm_group_cfg(FREQ_MAX, 2, frame, sizeof(frame)));

    /* 1000000 = 0x000F4240 */
    LONGS_EQUAL(0x40, frame[COMMAND_PAYLOAD_IDX + 0]);
    LONGS_EQUAL(0x42, frame[COMMAND_PAYLOAD_IDX + 1]);
    LONGS_EQUAL(0x0F, frame[COMMAND_PAYLOAD_IDX + 2]);
    LONGS_EQUAL(0x00, frame[COMMAND_PAYLOAD_IDX + 3]);
    LONGS_EQUAL(2, frame[COMMAND_PAYLOAD_IDX + 4]);
}

/* --- protocol_pwm_group_get --- */

// Section 12 of mcu-co_Protocol.md: "pwm group get 0".
TEST(Protocol, PwmGroupGetMatchesTheWorkedFrameInTheProtocolDoc)
{
    const uint8_t expected[] = {0xA5, 0x45, 0x01, 0x00, 0xF0, 0x09};
    uint8_t frame[PROTOCOL_MAX_COMMAND_FRAME] = {0};

    LONGS_EQUAL(sizeof(expected), protocol_pwm_group_get(0, frame, sizeof(frame)));
    MEMCMP_EQUAL(expected, frame, sizeof(expected));
}

TEST(Protocol, PwmGroupGetRejectsAGroupAboveTheMaximum)
{
    uint8_t frame[PROTOCOL_MAX_COMMAND_FRAME] = {0};

    LONGS_EQUAL(-STATUS_ERR_ARG,
                protocol_pwm_group_get(GROUP_MAX + 1, frame, sizeof(frame)));
}

/* --- protocol_gpio_irq_cfg --- */

// Section 4 of mcu-co_Protocol.md: "gpio irq cfg both A 5".
TEST(Protocol, GpioIrqCfgMatchesTheWorkedFrameInTheProtocolDoc)
{
    const uint8_t expected[] = {0xA5, 0x34, 0x03, 0x03, 0x00, 0x05, 0xCD, 0x06};
    uint8_t frame[PROTOCOL_MAX_COMMAND_FRAME] = {0};

    LONGS_EQUAL(sizeof(expected),
                protocol_gpio_irq_cfg(EDGE_BOTH, PORT_A, 5, frame, sizeof(frame)));
    MEMCMP_EQUAL(expected, frame, sizeof(expected));
}

// "gpio irq cfg off A 5" - EDGE of 0 disarms, and is legal here.
TEST(Protocol, GpioIrqCfgAcceptsOffToDisarm)
{
    const uint8_t expected[] = {0xA5, 0x34, 0x03, 0x00, 0x00, 0x05, 0x9D, 0x5F};
    uint8_t frame[PROTOCOL_MAX_COMMAND_FRAME] = {0};

    LONGS_EQUAL(sizeof(expected),
                protocol_gpio_irq_cfg(EDGE_OFF, PORT_A, 5, frame, sizeof(frame)));
    MEMCMP_EQUAL(expected, frame, sizeof(expected));
}

TEST(Protocol, GpioIrqCfgRejectsAnEdgeOutsideTheEncoding)
{
    uint8_t frame[PROTOCOL_MAX_COMMAND_FRAME] = {0};

    LONGS_EQUAL(-STATUS_ERR_ARG,
                protocol_gpio_irq_cfg((edge_t)4, PORT_A, 5, frame, sizeof(frame)));
}

/* --- protocol_gpio_irq_bind --- */

// Section 5: PC13 armed both edges toggling PA5. The widest payload, 6 bytes.
TEST(Protocol, GpioIrqBindMatchesTheWorkedFrameInTheProtocolDoc)
{
    const uint8_t expected[] = {0xA5, 0x33, 0x06, 0x03, 0x02, 0x0D,
                                0x02, 0x00, 0x05, 0x92, 0x93};
    uint8_t frame[PROTOCOL_MAX_COMMAND_FRAME] = {0};

    LONGS_EQUAL(sizeof(expected),
                protocol_gpio_irq_bind(EDGE_BOTH, PORT_C, 13, ACTION_TOGGLE, PORT_A, 5,
                                       frame, sizeof(frame)));
    MEMCMP_EQUAL(expected, frame, sizeof(expected));
}

// Dropping a binding is gpio irq unbind; disarming is gpio irq cfg off.
TEST(Protocol, GpioIrqBindRejectsAnEdgeOfOff)
{
    uint8_t frame[PROTOCOL_MAX_COMMAND_FRAME] = {0};

    LONGS_EQUAL(-STATUS_ERR_ARG,
                protocol_gpio_irq_bind(EDGE_OFF, PORT_C, 13, ACTION_TOGGLE, PORT_A, 5,
                                       frame, sizeof(frame)));
}

TEST(Protocol, GpioIrqBindRejectsAnActionOutsideTheEncoding)
{
    uint8_t frame[PROTOCOL_MAX_COMMAND_FRAME] = {0};

    LONGS_EQUAL(-STATUS_ERR_ARG,
                protocol_gpio_irq_bind(EDGE_BOTH, PORT_C, 13, (action_t)3, PORT_A, 5,
                                       frame, sizeof(frame)));
}

TEST(Protocol, GpioIrqBindValidatesBothPins)
{
    uint8_t frame[PROTOCOL_MAX_COMMAND_FRAME] = {0};

    LONGS_EQUAL(-STATUS_ERR_ARG,
                protocol_gpio_irq_bind(EDGE_BOTH, PORT_C, PIN_MAX + 1, ACTION_TOGGLE,
                                       PORT_A, 5, frame, sizeof(frame)));
    LONGS_EQUAL(-STATUS_ERR_ARG,
                protocol_gpio_irq_bind(EDGE_BOTH, PORT_C, 13, ACTION_TOGGLE, PORT_A,
                                       PIN_MAX + 1, frame, sizeof(frame)));
}

/* --- protocol_gpio_irq_unbind --- */

// Section 6: "gpio irq unbind A 5".
TEST(Protocol, GpioIrqUnbindMatchesTheWorkedFrameInTheProtocolDoc)
{
    const uint8_t expected[] = {0xA5, 0x35, 0x02, 0x00, 0x05, 0xA9, 0x2A};
    uint8_t frame[PROTOCOL_MAX_COMMAND_FRAME] = {0};

    LONGS_EQUAL(sizeof(expected),
                protocol_gpio_irq_unbind(PORT_A, 5, frame, sizeof(frame)));
    MEMCMP_EQUAL(expected, frame, sizeof(expected));
}

/* --- protocol_pwm_channel_cfg --- */

// Section 8 of mcu-co_Protocol.md: "pwm channel cfg high A 5".
TEST(Protocol, PwmChannelCfgMatchesTheWorkedFrameInTheProtocolDoc)
{
    const uint8_t expected[] = {0xA5, 0x41, 0x03, 0x00, 0x00, 0x05, 0x4C, 0x61};
    uint8_t frame[PROTOCOL_MAX_COMMAND_FRAME] = {0};

    LONGS_EQUAL(sizeof(expected),
                protocol_pwm_channel_cfg(POL_ACTIVE_HIGH, PORT_A, 5, frame, sizeof(frame)));
    MEMCMP_EQUAL(expected, frame, sizeof(expected));
}

TEST(Protocol, PwmChannelCfgRejectsAPolarityOutsideTheEncoding)
{
    uint8_t frame[PROTOCOL_MAX_COMMAND_FRAME] = {0};

    LONGS_EQUAL(-STATUS_ERR_ARG,
                protocol_pwm_channel_cfg((polarity_t)2, PORT_A, 5, frame, sizeof(frame)));
}

/* --- protocol_pwm_channel_set --- */

// Section 9: "pwm channel set 25.0 A 5". Duty is tenths of a percent, uint16 LE.
TEST(Protocol, PwmChannelSetMatchesTheWorkedFrameInTheProtocolDoc)
{
    const uint8_t expected[] = {0xA5, 0x42, 0x04, 0xFA, 0x00, 0x00, 0x05, 0x05, 0xC1};
    uint8_t frame[PROTOCOL_MAX_COMMAND_FRAME] = {0};

    LONGS_EQUAL(sizeof(expected),
                protocol_pwm_channel_set(250, PORT_A, 5, frame, sizeof(frame)));
    MEMCMP_EQUAL(expected, frame, sizeof(expected));
}

TEST(Protocol, PwmChannelSetAcceptsFullScale)
{
    uint8_t frame[PROTOCOL_MAX_COMMAND_FRAME] = {0};

    LONGS_EQUAL(9, protocol_pwm_channel_set(DUTY_MAX, PORT_A, 5, frame, sizeof(frame)));

    /* 1000 = 0x03E8 */
    LONGS_EQUAL(0xE8, frame[COMMAND_PAYLOAD_IDX + 0]);
    LONGS_EQUAL(0x03, frame[COMMAND_PAYLOAD_IDX + 1]);
}

TEST(Protocol, PwmChannelSetRejectsADutyAboveFullScale)
{
    uint8_t frame[PROTOCOL_MAX_COMMAND_FRAME] = {0};

    LONGS_EQUAL(-STATUS_ERR_ARG,
                protocol_pwm_channel_set(DUTY_MAX + 1, PORT_A, 5, frame, sizeof(frame)));
}

/* --- protocol_pwm_channel_get / release, protocol_pwm_group_release --- */

// Section 11: "pwm channel get A 5".
TEST(Protocol, PwmChannelGetMatchesTheWorkedFrameInTheProtocolDoc)
{
    const uint8_t expected[] = {0xA5, 0x44, 0x02, 0x00, 0x05, 0x68, 0x1E};
    uint8_t frame[PROTOCOL_MAX_COMMAND_FRAME] = {0};

    LONGS_EQUAL(sizeof(expected),
                protocol_pwm_channel_get(PORT_A, 5, frame, sizeof(frame)));
    MEMCMP_EQUAL(expected, frame, sizeof(expected));
}

// Section 10: "pwm channel release A 5".
TEST(Protocol, PwmChannelReleaseMatchesTheWorkedFrameInTheProtocolDoc)
{
    const uint8_t expected[] = {0xA5, 0x43, 0x02, 0x00, 0x05, 0x45, 0x4F};
    uint8_t frame[PROTOCOL_MAX_COMMAND_FRAME] = {0};

    LONGS_EQUAL(sizeof(expected),
                protocol_pwm_channel_release(PORT_A, 5, frame, sizeof(frame)));
    MEMCMP_EQUAL(expected, frame, sizeof(expected));
}

// Section 7.5: "pwm group release 0".
TEST(Protocol, PwmGroupReleaseMatchesTheWorkedFrameInTheProtocolDoc)
{
    const uint8_t expected[] = {0xA5, 0x46, 0x01, 0x00, 0xA0, 0x50};
    uint8_t frame[PROTOCOL_MAX_COMMAND_FRAME] = {0};

    LONGS_EQUAL(sizeof(expected), protocol_pwm_group_release(0, frame, sizeof(frame)));
    MEMCMP_EQUAL(expected, frame, sizeof(expected));
}

/* --- protocol_probe --- */

// Section 0 of mcu-co_Protocol.md: "probe". The only command with no payload.
TEST(Protocol, ProbeMatchesTheWorkedFrameInTheProtocolDoc)
{
    const uint8_t expected[] = {0xA5, 0x10, 0x00, 0x7C, 0x1E};
    uint8_t frame[PROTOCOL_MAX_COMMAND_FRAME] = {0};

    LONGS_EQUAL(sizeof(expected), protocol_probe(frame, sizeof(frame)));
    MEMCMP_EQUAL(expected, frame, sizeof(expected));
}

/* --- protocol_gpio_toggle --- */

// Section 6.5: "gpio toggle A 5". Payload is [PORT, PIN] - no qualifier,
// because unlike gpio set there is nothing to choose.
TEST(Protocol, GpioToggleMatchesTheWorkedFrameInTheProtocolDoc)
{
    const uint8_t expected[] = {0xA5, 0x36, 0x02, 0x00, 0x05, 0x75, 0xB1};
    uint8_t frame[PROTOCOL_MAX_COMMAND_FRAME] = {0};

    LONGS_EQUAL(sizeof(expected), protocol_gpio_toggle(PORT_A, 5, frame, sizeof(frame)));
    MEMCMP_EQUAL(expected, frame, sizeof(expected));
}

TEST(Protocol, GpioToggleRejectsAPinAboveTheMaximum)
{
    uint8_t frame[PROTOCOL_MAX_COMMAND_FRAME] = {0};

    LONGS_EQUAL(-STATUS_ERR_ARG,
                protocol_gpio_toggle(PORT_A, PIN_MAX + 1, frame, sizeof(frame)));
}

/* --- protocol_reset --- */

// Section 0.5 of mcu-co_Protocol.md: "reset". No payload, like probe.
TEST(Protocol, ResetMatchesTheWorkedFrameInTheProtocolDoc)
{
    const uint8_t expected[] = {0xA5, 0x11, 0x00, 0x4D, 0x2D};
    uint8_t frame[PROTOCOL_MAX_COMMAND_FRAME] = {0};

    LONGS_EQUAL(sizeof(expected), protocol_reset(frame, sizeof(frame)));
    MEMCMP_EQUAL(expected, frame, sizeof(expected));
}
