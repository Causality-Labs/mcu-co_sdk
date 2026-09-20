#include <signal.h>
#include <stdio.h>
#include <unistd.h>

#include "mcuco.h"

#define DEFAULT_DEVICE "/dev/ttyACM0"

/* Nucleo-G474RE: B1 user button on PC13, LD2 on PA5. */
#define BUTTON_PORT PORT_C
#define BUTTON_PIN  13
#define LED_PORT    PORT_A
#define LED_PIN     5

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

int main(int argc, char **argv)
{
    const char *device_path = (argc > 1) ? argv[1] : DEFAULT_DEVICE;

    install_signal_handlers();

    mcuco_t *mcu = mcuco_open(device_path, MCUCO_TIMEOUT_DEFAULT_MS);
    if (mcu == NULL)
    {
        perror("mcuco_open");
        return 1;
    }

    /* Set a pin as an input */
    mcu_status_t status = mcuco_gpio_cfg(mcu, DIR_INPUT, BUTTON_PORT, BUTTON_PIN);
    if (status != STATUS_OK)
    {
        fprintf(stderr, "%s\n", mcuco_strerror(status));
        return 1;
    }

    /* Set a pin as an output */
    status = mcuco_gpio_cfg(mcu, DIR_OUTPUT, LED_PORT, LED_PIN);
    if (status != STATUS_OK)
    {
        fprintf(stderr, "%s\n", mcuco_strerror(status));
        return 1;
    }

    /* Arm an interrupt on a pin */
    status = mcuco_gpio_irq_cfg(mcu, EDGE_BOTH, BUTTON_PORT, BUTTON_PIN);
    if (status != STATUS_OK)
    {
        fprintf(stderr, "%s\n", mcuco_strerror(status));
        return 1;
    }

    /* Bind the button's interrupt to toggle the LED */
    status = mcuco_gpio_irq_bind(mcu, EDGE_BOTH, BUTTON_PORT, BUTTON_PIN, ACTION_TOGGLE, LED_PORT, LED_PIN);
    if (status != STATUS_OK)
    {
        fprintf(stderr, "%s\n", mcuco_strerror(status));
        return 1;
    }

    fprintf(stderr, "bound - press B1. Ctrl-C to exit.\n");

    /* The MCU drives the LED from its own ISR now, so there is nothing for the
     * host to do. pause() sleeps until a signal arrives rather than spinning. */
    while (!stop_requested)
    {
        pause();
    }

    /* Note the tension here: a reset would put the pins back in a known state,
     * but it would also tear down the binding - which is the one thing this
     * example exists to leave running after the host exits. Uncomment only if
     * you want the MCU idle on exit.
     *
     * mcuco_reset(mcu);
     */

    mcuco_close(mcu);

    return 0;
}
