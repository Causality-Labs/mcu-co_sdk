#include "CppUTest/TestHarness.h"

#include <fcntl.h>
#include <pty.h>
#include <unistd.h>

extern "C" {
#include "mcuco.h"
}

/* "gpio cfg output A 5" and a bare ACK, from mcu-co_Protocol.md. */
static const uint8_t GPIO_CFG_FRAME[] = {0xA5, 0x30, 0x03, 0x01, 0x00, 0x05, 0xAB, 0xE1};
static const uint8_t ACK_FRAME[]      = {0xA5, 0x01, 0x01, 0x1F, 0x3E};
static const uint8_t NACK_BUSY[]      = {0xA5, 0x02, 0x00, 0x06, 0x3A, 0xC2};

TEST_GROUP(Mcuco)
{
    int      master;
    mcuco_t *mcu;

    void setup()
    {
        char name[128];
        int  slave;

        CHECK(openpty(&master, &slave, name, NULL, NULL) == 0);
        close(slave);

        mcu = mcuco_open(name, 500);
        CHECK(mcu != NULL);
    }

    void teardown()
    {
        mcuco_close(mcu);
        close(master);
    }

    /* Queue a reply so it is already waiting when the command is sent. */
    void reply_with(const uint8_t *frame, size_t len)
    {
        LONGS_EQUAL(len, write(master, frame, len));
    }
};

/* --- mcuco_gpio_cfg --- */

// The whole stack: typed arguments in, the doc's frame on the wire, the MCU's
// ACK decoded back into a status.
TEST(Mcuco, GpioCfgPutsTheDocumentedFrameOnTheWireAndAcceptsTheAck)
{
    uint8_t sent[sizeof(GPIO_CFG_FRAME)] = {0};

    reply_with(ACK_FRAME, sizeof(ACK_FRAME));

    LONGS_EQUAL(STATUS_OK, mcuco_gpio_cfg(mcu, DIR_OUTPUT, PORT_A, 5));

    LONGS_EQUAL(sizeof(GPIO_CFG_FRAME), read(master, sent, sizeof(GPIO_CFG_FRAME)));
    MEMCMP_EQUAL(GPIO_CFG_FRAME, sent, sizeof(GPIO_CFG_FRAME));
}

// A NACK's reason byte is the firmware's own status code, returned untranslated.
TEST(Mcuco, GpioCfgReturnsTheNackReasonFromTheMcu)
{
    reply_with(NACK_BUSY, sizeof(NACK_BUSY));

    LONGS_EQUAL(STATUS_ERR_BUSY, mcuco_gpio_cfg(mcu, DIR_OUTPUT, PORT_A, 5));
}

TEST(Mcuco, GpioCfgTimesOutWhenTheMcuSaysNothing)
{
    LONGS_EQUAL(STATUS_ERR_NO_RESPONSE, mcuco_gpio_cfg(mcu, DIR_OUTPUT, PORT_A, 5));
}

// Rejected locally, so nothing reaches the wire.
TEST(Mcuco, GpioCfgRejectsABadPinWithoutSendingAnything)
{
    uint8_t sent[1] = {0};

    LONGS_EQUAL(STATUS_ERR_ARG, mcuco_gpio_cfg(mcu, DIR_OUTPUT, PORT_A, PIN_MAX + 1));

    /* Non-blocking, or this read waits for bytes that were never sent. */
    fcntl(master, F_SETFL, O_NONBLOCK);
    LONGS_EQUAL(-1, read(master, sent, 1));
}

TEST(Mcuco, GpioCfgRejectsANullHandle)
{
    LONGS_EQUAL(STATUS_ERR_ARG, mcuco_gpio_cfg(NULL, DIR_OUTPUT, PORT_A, 5));
}

/* --- mcuco_gpio_set --- */

TEST(Mcuco, GpioSetPutsTheDocumentedFrameOnTheWireAndAcceptsTheAck)
{
    const uint8_t gpio_set_frame[] = {0xA5, 0x31, 0x03, 0x01, 0x00, 0x05, 0xFA, 0x4B};
    uint8_t sent[sizeof(gpio_set_frame)] = {0};

    reply_with(ACK_FRAME, sizeof(ACK_FRAME));

    LONGS_EQUAL(STATUS_OK, mcuco_gpio_set(mcu, LEVEL_HIGH, PORT_A, 5));

    LONGS_EQUAL(sizeof(gpio_set_frame), read(master, sent, sizeof(gpio_set_frame)));
    MEMCMP_EQUAL(gpio_set_frame, sent, sizeof(gpio_set_frame));
}

// Driving a pin that was never configured as an output is the MCU's call.
TEST(Mcuco, GpioSetReturnsInvalidStateWhenThePinIsNotAnOutput)
{
    const uint8_t nack_invalid_state[] = {0xA5, 0x02, 0x00, 0x04, 0x78, 0xE2};

    reply_with(nack_invalid_state, sizeof(nack_invalid_state));

    LONGS_EQUAL(STATUS_ERR_INVALID_STATE, mcuco_gpio_set(mcu, LEVEL_HIGH, PORT_A, 5));
}

/* --- mcuco_gpio_get --- */

// Section 3's response frame, where STATE carries the pin level.
TEST(Mcuco, GpioGetReturnsHighFromTheMcu)
{
    const uint8_t read_high[] = {0xA5, 0x02, 0x01, 0x01, 0xEC, 0x81};
    level_t level = LEVEL_LOW;

    reply_with(read_high, sizeof(read_high));

    LONGS_EQUAL(STATUS_OK, mcuco_gpio_get(mcu, PORT_A, 5, &level));
    LONGS_EQUAL(LEVEL_HIGH, level);
}

// Not a doc frame - the doc only works through a high reading - but its CRC
// comes from the same crc16 the doc's frames validate.
TEST(Mcuco, GpioGetReturnsLowFromTheMcu)
{
    const uint8_t read_low[] = {0xA5, 0x02, 0x01, 0x00, 0xCD, 0x91};
    level_t level = LEVEL_HIGH;

    reply_with(read_low, sizeof(read_low));

    LONGS_EQUAL(STATUS_OK, mcuco_gpio_get(mcu, PORT_A, 5, &level));
    LONGS_EQUAL(LEVEL_LOW, level);
}

// A NACK's DATA is a reason, never a reading - check ack before the value.
TEST(Mcuco, GpioGetDoesNotTreatANackReasonAsALevel)
{
    const uint8_t nack_invalid_state[] = {0xA5, 0x02, 0x00, 0x04, 0x78, 0xE2};
    level_t level = LEVEL_LOW;

    reply_with(nack_invalid_state, sizeof(nack_invalid_state));

    LONGS_EQUAL(STATUS_ERR_INVALID_STATE, mcuco_gpio_get(mcu, PORT_A, 5, &level));
    LONGS_EQUAL(LEVEL_LOW, level);   /* untouched */
}

TEST(Mcuco, GpioGetRejectsANullOutParameter)
{
    LONGS_EQUAL(STATUS_ERR_ARG, mcuco_gpio_get(mcu, PORT_A, 5, NULL));
}

/* --- mcuco_pwm_group_cfg --- */

TEST(Mcuco, PwmGroupCfgPutsTheDocumentedFrameOnTheWireAndAcceptsTheAck)
{
    const uint8_t expected[] = {0xA5, 0x40, 0x05, 0xE8, 0x03, 0x00, 0x00, 0x00, 0xDE, 0xCD};
    uint8_t sent[sizeof(expected)] = {0};

    reply_with(ACK_FRAME, sizeof(ACK_FRAME));

    LONGS_EQUAL(STATUS_OK, mcuco_pwm_group_cfg(mcu, 1000, 0));

    LONGS_EQUAL(sizeof(expected), read(master, sent, sizeof(expected)));
    MEMCMP_EQUAL(expected, sent, sizeof(expected));
}

// Reconfiguring a live group is refused, leaving it running untouched.
TEST(Mcuco, PwmGroupCfgReturnsBusyForAGroupAlreadyConfigured)
{
    reply_with(NACK_BUSY, sizeof(NACK_BUSY));

    LONGS_EQUAL(STATUS_ERR_BUSY, mcuco_pwm_group_cfg(mcu, 1000, 0));
}

/* --- mcuco_pwm_group_get --- */

// Section 12's response: a uint32 frequency, little-endian, LEN of 5.
TEST(Mcuco, PwmGroupGetReturnsTheAchievedFrequency)
{
    const uint8_t freq_1khz[] = {0xA5, 0x05, 0x01, 0xE8, 0x03, 0x00, 0x00, 0x39, 0xBF};
    uint32_t achieved_hz = 0;

    reply_with(freq_1khz, sizeof(freq_1khz));

    LONGS_EQUAL(STATUS_OK, mcuco_pwm_group_get(mcu, 0, &achieved_hz));
    LONGS_EQUAL(1000, achieved_hz);
}

// A group with no frequency configured refuses rather than reporting zero.
TEST(Mcuco, PwmGroupGetReturnsNotInitForAnUnconfiguredGroup)
{
    const uint8_t nack_not_init[] = {0xA5, 0x02, 0x00, 0x05, 0x59, 0xF2};
    uint32_t achieved_hz = 12345;

    reply_with(nack_not_init, sizeof(nack_not_init));

    LONGS_EQUAL(STATUS_ERR_NOT_INIT, mcuco_pwm_group_get(mcu, 0, &achieved_hz));
    LONGS_EQUAL(12345, achieved_hz);   /* untouched */
}

// A four-byte read must not be satisfied by a one-byte frame.
TEST(Mcuco, PwmGroupGetRejectsAResponseOfTheWrongWidth)
{
    const uint8_t read_high[] = {0xA5, 0x02, 0x01, 0x01, 0xEC, 0x81};
    uint32_t achieved_hz = 0;

    reply_with(read_high, sizeof(read_high));

    LONGS_EQUAL(STATUS_ERR_BAD_FRAME, mcuco_pwm_group_get(mcu, 0, &achieved_hz));
}

TEST(Mcuco, PwmGroupGetRejectsANullOutParameter)
{
    LONGS_EQUAL(STATUS_ERR_ARG, mcuco_pwm_group_get(mcu, 0, NULL));
}

/* --- the remaining commands, end to end --- */

TEST(Mcuco, PwmChannelSetPutsTheDocumentedFrameOnTheWire)
{
    const uint8_t expected[] = {0xA5, 0x42, 0x04, 0xFA, 0x00, 0x00, 0x05, 0x05, 0xC1};
    uint8_t sent[sizeof(expected)] = {0};

    reply_with(ACK_FRAME, sizeof(ACK_FRAME));

    LONGS_EQUAL(STATUS_OK, mcuco_pwm_channel_set(mcu, 250, PORT_A, 5));

    LONGS_EQUAL(sizeof(expected), read(master, sent, sizeof(expected)));
    MEMCMP_EQUAL(expected, sent, sizeof(expected));
}

// Section 11's response: a uint16 duty, little-endian, LEN of 3.
TEST(Mcuco, PwmChannelGetReturnsTheDuty)
{
    const uint8_t duty_250[] = {0xA5, 0x03, 0x01, 0xFA, 0x00, 0x26, 0xD4};
    uint16_t duty = 0;

    reply_with(duty_250, sizeof(duty_250));

    LONGS_EQUAL(STATUS_OK, mcuco_pwm_channel_get(mcu, PORT_A, 5, &duty));
    LONGS_EQUAL(250, duty);
}

// Claiming a pin whose group has no frequency yet.
TEST(Mcuco, PwmChannelCfgReturnsNotInitBeforeTheGroupIsConfigured)
{
    const uint8_t nack_not_init[] = {0xA5, 0x02, 0x00, 0x05, 0x59, 0xF2};

    reply_with(nack_not_init, sizeof(nack_not_init));

    LONGS_EQUAL(STATUS_ERR_NOT_INIT,
                mcuco_pwm_channel_cfg(mcu, POL_ACTIVE_HIGH, PORT_A, 5));
}

TEST(Mcuco, GpioIrqBindPutsTheDocumentedFrameOnTheWire)
{
    const uint8_t expected[] = {0xA5, 0x33, 0x06, 0x03, 0x02, 0x0D,
                                0x02, 0x00, 0x05, 0x92, 0x93};
    uint8_t sent[sizeof(expected)] = {0};

    reply_with(ACK_FRAME, sizeof(ACK_FRAME));

    LONGS_EQUAL(STATUS_OK,
                mcuco_gpio_irq_bind(mcu, EDGE_BOTH, PORT_C, 13, ACTION_TOGGLE, PORT_A, 5));

    LONGS_EQUAL(sizeof(expected), read(master, sent, sizeof(expected)));
    MEMCMP_EQUAL(expected, sent, sizeof(expected));
}
