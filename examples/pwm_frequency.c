#include <signal.h>
#include <stdio.h>
#include <unistd.h>

#include "mcuco.h"

#define DEFAULT_DEVICE "/dev/ttyACM0"

/* One group per frequency: reconfiguring a live group is refused, so three
 * frequencies need all three timers. */
#define GROUP_1KHZ  0
#define GROUP_60KHZ 1
#define GROUP_7777  2

/* Set from a signal handler, so it must be sig_atomic_t and volatile. */
static volatile sig_atomic_t stop_requested = 0;

/* Handlers may only call async-signal-safe functions, which rules out talking
 * to the MCU from in here - that needs malloc'd state and a round trip. Raise
 * a flag and let main() unwind normally instead. */
static void request_stop(int signal_number)
{
    (void)signal_number;

    stop_requested = 1;
}

static void install_signal_handlers(void)
{
    struct sigaction action;

    action.sa_handler = request_stop;
    action.sa_flags   = 0;
    sigemptyset(&action.sa_mask);

    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);
}

/* The prescaler and reload are integers, so the frequency the hardware lands
 * on is not always the one asked for. This is the only way to learn the real
 * figure. */
static void report_group(mcuco_t *mcu, uint8_t group, uint32_t requested_hz)
{
    mcu_status_t status = mcuco_pwm_group_cfg(mcu, requested_hz, group);

    /* ERR_BUSY leaves the group running at whatever it was already set to, so
     * the read below still says something true. */
    if (status != STATUS_OK && status != STATUS_ERR_BUSY)
    {
        fprintf(stderr, "group %u cfg: %s\n", group, mcuco_strerror(status));
        return;
    }

    uint32_t achieved_hz = 0;

    status = mcuco_pwm_group_get(mcu, group, &achieved_hz);
    if (status != STATUS_OK)
    {
        fprintf(stderr, "group %u get: %s\n", group, mcuco_strerror(status));
        return;
    }

    long drift_hz = (long)achieved_hz - (long)requested_hz;

    printf("group %u: requested %7u Hz -> achieved %7u Hz  (%+ld)\n", group, requested_hz, achieved_hz, drift_hz);
}

int main(int argc, char **argv)
{
    const char *device_path = (argc > 1) ? argv[1] : DEFAULT_DEVICE;

    install_signal_handlers();

    fprintf(stderr, "opening %s\n", device_path);

    mcuco_t *mcu = mcuco_open(device_path, MCUCO_TIMEOUT_DEFAULT_MS);
    if (mcu == NULL)
    {
        perror("mcuco_open");
        return 1;
    }

    /* 1 kHz divides the 170 MHz timer clock exactly; the other two do not. */
    report_group(mcu, GROUP_1KHZ, 1000);

    if (!stop_requested)
    {
        report_group(mcu, GROUP_60KHZ, 60000);
    }

    if (!stop_requested)
    {
        report_group(mcu, GROUP_7777, 7777);
    }

    /* Hand the pins back to a known state before dropping the link - the
     * timers keep running until the MCU is reset or repowered. Waiting on the
     * firmware command:
     *
     * mcuco_reset(mcu);
     */

    mcuco_close(mcu);

    return 0;
}
