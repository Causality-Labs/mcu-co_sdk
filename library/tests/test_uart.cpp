#include "CppUTest/TestHarness.h"

#include <errno.h>
#include <pty.h>
#include <time.h>
#include <unistd.h>

extern "C" {
#include "uart.h"
}

/* An ACK and a "gpio set high A 5" command, taken from mcu-co_Protocol.md. */
static const uint8_t ACK_FRAME[]      = {0xA5, 0x01, 0x01, 0x1F, 0x3E};
static const uint8_t GPIO_SET_FRAME[] = {0xA5, 0x31, 0x03, 0x01, 0x00, 0x05, 0xFA, 0x4B};

static long now_ms(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    long milliseconds = (now.tv_sec * 1000L) + (now.tv_nsec / 1000000L);

    return milliseconds;
}

/* Writes one byte every interval_ms from a forked child, so the parent sees a
 * frame arrive in pieces the way a real link delivers it. */
static void write_slowly(int fd, const uint8_t *buf, size_t len, unsigned interval_ms)
{
    if (fork() != 0)
        return;

    for (size_t i = 0; i < len; i++)
    {
        usleep(interval_ms * 1000);
        ssize_t written = write(fd, &buf[i], 1);
        (void)written;
    }

    _exit(0);
}

TEST_GROUP(Uart)
{
    int master;
    int port;

    void setup()
    {
        char name[128];
        int slave;

        CHECK(openpty(&master, &slave, name, NULL, NULL) == 0);
        close(slave);

        /* Reopen through uart_open: a default pty is canonical, so a frame
         * with no newline would never arrive. */
        port = uart_open(name);
        CHECK(port >= 0);
    }

    void teardown()
    {
        uart_close(port);
        close(master);
    }
};

/* --- uart_open --- */

TEST(Uart, OpenReportsErrnoForAMissingDevice)
{
    LONGS_EQUAL(-ENOENT, uart_open("/dev/tty-does-not-exist"));
}

/* --- uart_close --- */

TEST(Uart, CloseReleasesTheDescriptor)
{
    int duplicate = dup(port);

    uart_close(duplicate);

    LONGS_EQUAL(-EBADF, uart_write(duplicate, GPIO_SET_FRAME, sizeof(GPIO_SET_FRAME)));
}

// uart_open reports failure as a negative errno, so that value reaching
// uart_close must be ignored rather than passed to close().
TEST(Uart, CloseIgnoresANegativeDescriptor)
{
    uart_close(-1);
    uart_close(uart_open("/dev/tty-does-not-exist"));
}

/* --- uart_read --- */

TEST(Uart, ReadsAWholeFrame)
{
    uint8_t buf[sizeof(ACK_FRAME)] = {0};

    ssize_t written = write(master, ACK_FRAME, sizeof(ACK_FRAME));
    LONGS_EQUAL(sizeof(ACK_FRAME), written);

    LONGS_EQUAL(sizeof(ACK_FRAME), uart_read(port, buf, sizeof(ACK_FRAME), 500));
    MEMCMP_EQUAL(ACK_FRAME, buf, sizeof(ACK_FRAME));
}

TEST(Uart, ReadTimesOutWhenNothingArrives)
{
    uint8_t buf[sizeof(ACK_FRAME)] = {0};

    long started = now_ms();
    LONGS_EQUAL(-ETIMEDOUT, uart_read(port, buf, sizeof(ACK_FRAME), 200));
    long elapsed = now_ms() - started;

    CHECK(elapsed >= 180);
    CHECK(elapsed < 400);
}

// The doc lists "bytes delivered one-per-interrupt" as a case worth fuzzing:
// a frame split across many reads must still assemble.
TEST(Uart, ReadAssemblesAFrameDeliveredOneByteAtATime)
{
    uint8_t buf[sizeof(ACK_FRAME)] = {0};

    write_slowly(master, ACK_FRAME, sizeof(ACK_FRAME), 20);

    LONGS_EQUAL(sizeof(ACK_FRAME), uart_read(port, buf, sizeof(ACK_FRAME), 2000));
    MEMCMP_EQUAL(ACK_FRAME, buf, sizeof(ACK_FRAME));
}

// The timeout bounds the whole call, so a partial read must not restart it.
// Five bytes 40ms apart cannot fit in 100ms, however many reads it takes.
TEST(Uart, ReadDeadlineIsNotResetByAPartialRead)
{
    uint8_t buf[sizeof(ACK_FRAME)] = {0};

    write_slowly(master, ACK_FRAME, sizeof(ACK_FRAME), 40);

    long started = now_ms();
    LONGS_EQUAL(-ETIMEDOUT, uart_read(port, buf, sizeof(ACK_FRAME), 100));
    long elapsed = now_ms() - started;

    CHECK(elapsed < 250);
}

/* --- uart_write --- */

TEST(Uart, WritesTheExactBytesToThePort)
{
    uint8_t echoed[sizeof(GPIO_SET_FRAME)] = {0};

    LONGS_EQUAL(sizeof(GPIO_SET_FRAME),
                uart_write(port, GPIO_SET_FRAME, sizeof(GPIO_SET_FRAME)));

    ssize_t read_back = read(master, echoed, sizeof(GPIO_SET_FRAME));
    LONGS_EQUAL(sizeof(GPIO_SET_FRAME), read_back);
    MEMCMP_EQUAL(GPIO_SET_FRAME, echoed, sizeof(GPIO_SET_FRAME));
}

TEST(Uart, WriteReportsErrnoForAClosedDescriptor)
{
    int closed = dup(port);
    close(closed);

    LONGS_EQUAL(-EBADF, uart_write(closed, GPIO_SET_FRAME, sizeof(GPIO_SET_FRAME)));
}

// Responses carry no opcode, so a second handle reading the first one's replies
// would be undetectable. The port is claimed exclusively instead.
TEST(Uart, OpenRefusesASecondHandleOnTheSameDevice)
{
    char name[128];
    int  probe_master;
    int  slave;

    CHECK(openpty(&probe_master, &slave, name, NULL, NULL) == 0);
    close(slave);

    int first = uart_open(name);
    CHECK(first >= 0);

    LONGS_EQUAL(-EBUSY, uart_open(name));

    uart_close(first);
    close(probe_master);
}
