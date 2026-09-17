#include "CppUTest/TestHarness.h"

#include <errno.h>
#include <string.h>
#include <pty.h>
#include <unistd.h>

extern "C" {
#include "command_transport.h"
#include "protocol.h"
}

/* "gpio cfg output A 5" from section 1 of mcu-co_Protocol.md. */
static const uint8_t GPIO_CFG_FRAME[] = {0xA5, 0x30, 0x03, 0x01, 0x00, 0x05, 0xAB, 0xE1};

TEST_GROUP(CommandTransport)
{
    int master;
    int port;

    void setup()
    {
        char name[128];
        int slave;

        CHECK(openpty(&master, &slave, name, NULL, NULL) == 0);
        close(slave);

        port = open_port(name);
        CHECK(port >= 0);
    }

    void teardown()
    {
        close_port(port);
        close(master);
    }
};

/* --- transmit_command --- */

TEST(CommandTransport, TransmitPutsTheExactFrameOnThePort)
{
    uint8_t echoed[sizeof(GPIO_CFG_FRAME)] = {0};

    LONGS_EQUAL(STATUS_OK,
                transmit_command(port, GPIO_CFG_FRAME, sizeof(GPIO_CFG_FRAME)));

    LONGS_EQUAL(sizeof(GPIO_CFG_FRAME), read(master, echoed, sizeof(GPIO_CFG_FRAME)));
    MEMCMP_EQUAL(GPIO_CFG_FRAME, echoed, sizeof(GPIO_CFG_FRAME));
}

TEST(CommandTransport, TransmitRejectsANullFrame)
{
    LONGS_EQUAL(STATUS_ERR_ARG, transmit_command(port, NULL, sizeof(GPIO_CFG_FRAME)));
}

TEST(CommandTransport, TransmitRejectsAnEmptyFrame)
{
    LONGS_EQUAL(STATUS_ERR_ARG, transmit_command(port, GPIO_CFG_FRAME, 0));
}

TEST(CommandTransport, TransmitReportsFailureForAClosedDescriptor)
{
    int closed = dup(port);
    close(closed);

    LONGS_EQUAL(STATUS_ERR_IO,
                transmit_command(closed, GPIO_CFG_FRAME, sizeof(GPIO_CFG_FRAME)));
}

/* --- receive_response --- */

static const uint8_t ACK_FRAME[]  = {0xA5, 0x01, 0x01, 0x1F, 0x3E};
static const uint8_t FREQ_FRAME[] = {0xA5, 0x05, 0x01, 0xE8, 0x03, 0x00, 0x00, 0x39, 0xBF};
static const uint8_t NACK_BUSY[]  = {0xA5, 0x02, 0x00, 0x06, 0x3A, 0xC2};

TEST(CommandTransport, ReceivesABareAck)
{
    protocol_response_t response = {};

    LONGS_EQUAL(sizeof(ACK_FRAME), write(master, ACK_FRAME, sizeof(ACK_FRAME)));

    LONGS_EQUAL(STATUS_OK, receive_response(port, 500, &response));
    CHECK_TRUE(response.ack);
    LONGS_EQUAL(0, response.data_len);
}

// The widest response: LEN of 5 carries a uint32 frequency.
TEST(CommandTransport, ReceivesTheWidestResponse)
{
    protocol_response_t response = {};

    LONGS_EQUAL(sizeof(FREQ_FRAME), write(master, FREQ_FRAME, sizeof(FREQ_FRAME)));

    LONGS_EQUAL(STATUS_OK, receive_response(port, 500, &response));
    LONGS_EQUAL(4, response.data_len);
}

TEST(CommandTransport, ReceivesANackWithoutTreatingTheReasonAsData)
{
    protocol_response_t response = {};

    LONGS_EQUAL(sizeof(NACK_BUSY), write(master, NACK_BUSY, sizeof(NACK_BUSY)));

    LONGS_EQUAL(STATUS_OK, receive_response(port, 500, &response));
    CHECK_FALSE(response.ack);
    LONGS_EQUAL(STATUS_ERR_BUSY, response.data[0]);
}

// SOF is a resync anchor: stale bytes from an abandoned exchange are skipped.
TEST(CommandTransport, SkipsJunkBeforeTheStartByte)
{
    const uint8_t junk[] = {0x00, 0xFF, 0x42};
    protocol_response_t response = {};

    LONGS_EQUAL(sizeof(junk), write(master, junk, sizeof(junk)));
    LONGS_EQUAL(sizeof(ACK_FRAME), write(master, ACK_FRAME, sizeof(ACK_FRAME)));

    LONGS_EQUAL(STATUS_OK, receive_response(port, 500, &response));
    CHECK_TRUE(response.ack);
}

TEST(CommandTransport, TimesOutWhenNothingArrives)
{
    protocol_response_t response = {};

    LONGS_EQUAL(STATUS_ERR_NO_RESPONSE, receive_response(port, 150, &response));
}

// A corrupt frame is rejected rather than reported as a refusal.
TEST(CommandTransport, RejectsAFrameWithABadCrc)
{
    uint8_t corrupt[sizeof(ACK_FRAME)];
    protocol_response_t response = {};

    memcpy(corrupt, ACK_FRAME, sizeof(ACK_FRAME));
    corrupt[2] ^= 0x01;

    LONGS_EQUAL(sizeof(corrupt), write(master, corrupt, sizeof(corrupt)));

    LONGS_EQUAL(STATUS_ERR_BAD_FRAME, receive_response(port, 500, &response));
}

// The deadline bounds the whole exchange, not each read.
TEST(CommandTransport, DeadlineCoversTheWholeFrameNotEachRead)
{
    protocol_response_t response = {};

    LONGS_EQUAL(2, write(master, ACK_FRAME, 2));   /* SOF and LEN only */

    LONGS_EQUAL(STATUS_ERR_NO_RESPONSE, receive_response(port, 150, &response));
}
