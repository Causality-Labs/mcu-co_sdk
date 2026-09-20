#include <signal.h>
#include <stdio.h>
#include <time.h>

#include "mcuco.h"

#define DEFAULT_DEVICE "/dev/ttyACM0"

/* Nucleo-G474RE: LD2 is PA5, which is channel 1 of group 0 (TIM2). */
#define LED_PORT  PORT_A
#define LED_PIN   5
#define LED_GROUP 0

/* Fast enough that the eye sees a steady brightness rather than flicker. */
#define PWM_HZ 1000

/* 50 steps a second: one full ramp takes a second, and each step is a round
 * trip to the MCU. */
#define DUTY_STEP 20
#define STEP_MS   20
#define STEP_NS   (STEP_MS * 1000000L)

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

static void wait_a_step(void)
{
    struct timespec step = {.tv_sec = 0, .tv_nsec = STEP_NS};

    nanosleep(&step, NULL);
}

/* A group keeps its frequency until it is released, so a previous run that did
 * not reset the MCU leaves this one refused. Take the group over rather than
 * making the user power-cycle the board. */
static mcu_status_t claim_group(mcuco_t *mcu)
{
    mcu_status_t status = mcuco_pwm_group_cfg(mcu, PWM_HZ, LED_GROUP);

    if (status == STATUS_ERR_BUSY)
    {
        fprintf(stderr, "group %u was already running, taking it over\n", LED_GROUP);

        status = mcuco_pwm_group_release(mcu, LED_GROUP);
        if (status != STATUS_OK)
        {
            return status;
        }

        status = mcuco_pwm_group_cfg(mcu, PWM_HZ, LED_GROUP);
    }

    return status;
}

/* Duty is in tenths of a percent, so the ramp runs 0 to DUTY_MAX. Brightness
 * looks non-linear to the eye at a linear duty - fine for a demo. */
static mcu_status_t ramp(mcuco_t *mcu, int from, int to)
{
    int step = (to > from) ? DUTY_STEP : -DUTY_STEP;
    int duty = from;

    while (!stop_requested)
    {
        mcu_status_t status = mcuco_pwm_channel_set(mcu, (uint16_t)duty, LED_PORT, LED_PIN);
        if (status != STATUS_OK)
        {
            return status;
        }

        if (duty == to)
        {
            break;
        }

        /* Clamp rather than assume the step divides the range evenly: a step
         * that overshoots would otherwise never equal `to`. */
        duty += step;
        if (((step > 0) && (duty > to)) || ((step < 0) && (duty < to)))
        {
            duty = to;
        }

        wait_a_step();
    }

    return STATUS_OK;
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

    mcu_status_t status = claim_group(mcu);
    if (status != STATUS_OK)
    {
        fprintf(stderr, "pwm group cfg: %s\n", mcuco_strerror(status));
        mcuco_close(mcu);
        return 1;
    }

    /* Claiming the pin leaves it silent at 0.0% until the first set below. */
    status = mcuco_pwm_channel_cfg(mcu, POL_ACTIVE_HIGH, LED_PORT, LED_PIN);
    if (status != STATUS_OK)
    {
        fprintf(stderr, "pwm channel cfg: %s\n", mcuco_strerror(status));
        mcuco_close(mcu);
        return 1;
    }

    fprintf(stderr, "breathing LD2 at %u Hz. Ctrl-C to exit.\n", PWM_HZ);

    while (!stop_requested && status == STATUS_OK)
    {
        status = ramp(mcu, 0, DUTY_MAX);
        if (status != STATUS_OK)
        {
            break;
        }

        status = ramp(mcu, DUTY_MAX, 0);
    }

    if (status != STATUS_OK)
    {
        fprintf(stderr, "pwm channel set: %s\n", mcuco_strerror(status));
    }

    /* The timer keeps driving the pin until the MCU is reset. Nothing works on
     * this handle afterwards, so closing is all that is left. */
    mcu_status_t reset_status = mcuco_reset(mcu);
    if (reset_status != STATUS_OK)
    {
        fprintf(stderr, "reset: %s\n", mcuco_strerror(reset_status));
    }

    mcuco_close(mcu);

    return (status == STATUS_OK) ? 0 : 1;
}
