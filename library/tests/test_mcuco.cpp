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
