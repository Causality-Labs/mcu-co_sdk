#include <stdio.h>
#include "mcuco_command.h"

static void print_level(level_t level)
{
    printf("%s\n", (level == LEVEL_HIGH) ? "high" : "low");
}

static mcu_status_t run_mcu(mcuco_t *mcu, const mcu_system_t *mcu_system)
{
    switch (mcu_system->verb)
    {
    case PROBE:
        return mcuco_probe(mcu);

    case RESET:
        return mcuco_reset(mcu);

    default:
        return STATUS_ERR_ARG;
    }
}

static mcu_status_t run_gpio(mcuco_t *mcu, const gpio_system_t *gpio)
{
    level_t level = LEVEL_LOW;
    mcu_status_t status;

    switch (gpio->verb)
    {
    case CONFIG:
        return mcuco_gpio_cfg(mcu, gpio->direction, gpio->port, gpio->pin);

    case SET:
        return mcuco_gpio_set(mcu, gpio->level, gpio->port, gpio->pin);

    case GET:
        status = mcuco_gpio_get(mcu, gpio->port, gpio->pin, &level);
        if (status == STATUS_OK)
        {
            print_level(level);
        }
        return status;

    case TOGGLE:
        status = mcuco_gpio_toggle(mcu, gpio->port, gpio->pin, &level);
        if (status == STATUS_OK)
        {
            print_level(level);
        }
        return status;

    default:
        return STATUS_ERR_ARG;
    }
}

static mcu_status_t run_irq(mcuco_t *mcu, const irq_system_t *irq)
{
    (void)mcu;
    (void)irq;

    return STATUS_ERR_UNSUPPORTED;
}

static mcu_status_t run_timer(mcuco_t *mcu, const timer_system_t *timer)
{
    (void)mcu;
    (void)timer;

    return STATUS_ERR_UNSUPPORTED;
}

static mcu_status_t run_pwm(mcuco_t *mcu, const pwm_system_t *pwm)
{
    (void)mcu;
    (void)pwm;

    return STATUS_ERR_UNSUPPORTED;
}

mcu_status_t mcuco_run_command(mcuco_t *mcu, const mcuco_args_t *mcuco_args)
{
    switch (mcuco_args->subsytem)
    {
    case MCU:
        return run_mcu(mcu, &mcuco_args->system.mcu);

    case GPIO:
        return run_gpio(mcu, &mcuco_args->system.gpio);

    case IRQ:
        return run_irq(mcu, &mcuco_args->system.irq);

    case TIMER:
        return run_timer(mcu, &mcuco_args->system.timer);

    case PWM:
        return run_pwm(mcu, &mcuco_args->system.pwm);

    default:
        return STATUS_ERR_ARG;
    }
}
