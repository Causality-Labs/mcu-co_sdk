#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

#include "mcuco.h"

#define DEFAULT_DEVICE "/dev/ttyACM0"

/* Set from a signal handler, so it must be sig_atomic_t and volatile. */
static volatile sig_atomic_t stop_requested = 0;

/* Handlers may only call async-signal-safe functions, which rules out talking
 * to the MCU from in here - that needs malloc'd state and a round trip. Raise
 * a flag and let the loop below unwind normally instead. */
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

    mcu_status_t status = mcuco_gpio_cfg(mcu, DIR_OUTPUT, PORT_A, 5);
    if (status != STATUS_OK)
    {
        fprintf(stderr, "%s\n", mcuco_strerror(status));
        return 1;
    }

    /* PA5 is LD2 on the Nucleo, so this is visible. Ctrl-C drops out of the
     * loop rather than killing the process mid-blink, which would leave the
     * LED wherever it happened to be. */
    while (!stop_requested)
    {
        status = mcuco_gpio_set(mcu, LEVEL_HIGH, PORT_A, 5);
        if (status != STATUS_OK)
        {
            fprintf(stderr, "%s\n", mcuco_strerror(status));
            break;
        }

        sleep(1);

        status = mcuco_gpio_set(mcu, LEVEL_LOW, PORT_A, 5);
        if (status != STATUS_OK)
        {
            fprintf(stderr, "%s\n", mcuco_strerror(status));
            break;
        }
        sleep(1);
    }

    /* Hand the pins back to a known state before dropping the link - the MCU
     * keeps driving whatever it was told to until it is reset. Nothing works
     * on this handle afterwards, so closing is all that is left. */
    mcu_status_t reset_status = mcuco_reset(mcu);
    if (reset_status != STATUS_OK)
    {
        fprintf(stderr, "reset: %s\n", mcuco_strerror(reset_status));
    }

    mcuco_close(mcu);

    return (status == STATUS_OK) ? 0 : 1;
}
