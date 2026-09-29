#include <string.h>

#include "mcuco_spy.h"

/* The real definition is in library/src/mcuco.c, which cli_tests does not link,
 * so the spy is free to give the opaque handle a body of its own. */
struct mcuco
{
    int unused;
};

static mcuco_spy_t spy;
static struct mcuco handle;

void mcuco_spy_reset(void)
{
    (void)memset(&spy, 0, sizeof(spy));

    spy.next_status = STATUS_OK;
    spy.next_level  = LEVEL_LOW;
}

mcuco_spy_t *mcuco_spy(void)
{
    return &spy;
}

mcuco_t *mcuco_spy_handle(void)
{
    return &handle;
}

static mcu_status_t record(mcuco_call_t call, const mcuco_t *mcu)
{
    spy.calls++;
    spy.last_call = call;
    spy.handle    = mcu;

    return spy.next_status;
}

mcu_status_t mcuco_probe(mcuco_t *mcu)
{
    return record(CALL_PROBE, mcu);
}

mcu_status_t mcuco_reset(mcuco_t *mcu)
{
    return record(CALL_RESET, mcu);
}

mcu_status_t mcuco_gpio_cfg(mcuco_t *mcu, dir_t dir, port_t port, uint8_t pin)
{
    spy.direction = dir;
    spy.port      = port;
    spy.pin       = pin;

    return record(CALL_GPIO_CFG, mcu);
}

mcu_status_t mcuco_gpio_set(mcuco_t *mcu, level_t level, port_t port, uint8_t pin)
{
    spy.level = level;
    spy.port  = port;
    spy.pin   = pin;

    return record(CALL_GPIO_SET, mcu);
}

/* The real library leaves `level` untouched unless STATUS_OK is returned, so the
 * spy does too - otherwise a test could pass against a wrapper that prints a
 * value the MCU never sent. */
mcu_status_t mcuco_gpio_get(mcuco_t *mcu, port_t port, uint8_t pin, level_t *level)
{
    spy.port = port;
    spy.pin  = pin;

    mcu_status_t status = record(CALL_GPIO_GET, mcu);
    if (status == STATUS_OK)
    {
        *level = spy.next_level;
    }

    return status;
}

mcu_status_t mcuco_gpio_toggle(mcuco_t *mcu, port_t port, uint8_t pin, level_t *level)
{
    spy.port = port;
    spy.pin  = pin;

    mcu_status_t status = record(CALL_GPIO_TOGGLE, mcu);
    if (status == STATUS_OK)
    {
        *level = spy.next_level;
    }

    return status;
}

mcu_status_t mcuco_pwm_group_cfg(mcuco_t *mcu, uint32_t freq_hz, uint8_t group)
{
    spy.frequency_hz = freq_hz;
    spy.group        = group;

    return record(CALL_PWM_GROUP_CFG, mcu);
}

/* Written only on STATUS_OK, like the real library's achieved_hz. */
mcu_status_t mcuco_pwm_group_get(mcuco_t *mcu, uint8_t group, uint32_t *achieved_hz)
{
    spy.group = group;

    mcu_status_t status = record(CALL_PWM_GROUP_GET, mcu);
    if (status == STATUS_OK)
    {
        *achieved_hz = spy.next_frequency_hz;
    }

    return status;
}

mcu_status_t mcuco_pwm_group_release(mcuco_t *mcu, uint8_t group)
{
    spy.group = group;

    return record(CALL_PWM_GROUP_RELEASE, mcu);
}

mcu_status_t mcuco_pwm_channel_cfg(mcuco_t *mcu, polarity_t polarity, port_t port, uint8_t pin)
{
    spy.polarity = polarity;
    spy.port     = port;
    spy.pin      = pin;

    return record(CALL_PWM_CHANNEL_CFG, mcu);
}

mcu_status_t mcuco_pwm_channel_set(mcuco_t *mcu, uint16_t duty, port_t port, uint8_t pin)
{
    spy.duty = duty;
    spy.port = port;
    spy.pin  = pin;

    return record(CALL_PWM_CHANNEL_SET, mcu);
}

/* Written only on STATUS_OK, like the real library's duty. */
mcu_status_t mcuco_pwm_channel_get(mcuco_t *mcu, port_t port, uint8_t pin, uint16_t *duty)
{
    spy.port = port;
    spy.pin  = pin;

    mcu_status_t status = record(CALL_PWM_CHANNEL_GET, mcu);
    if (status == STATUS_OK)
    {
        *duty = spy.next_duty;
    }

    return status;
}

mcu_status_t mcuco_pwm_channel_release(mcuco_t *mcu, port_t port, uint8_t pin)
{
    spy.port = port;
    spy.pin  = pin;

    return record(CALL_PWM_CHANNEL_RELEASE, mcu);
}

mcu_status_t mcuco_gpio_irq_cfg(mcuco_t *mcu, edge_t edge, port_t port, uint8_t pin)
{
    spy.edge = edge;
    spy.port = port;
    spy.pin  = pin;

    return record(CALL_IRQ_CFG, mcu);
}

/* The trigger pin goes in port / pin and the driven pin in out_port / out_pin,
 * so a test can tell them apart. */
mcu_status_t mcuco_gpio_irq_bind(mcuco_t *mcu, edge_t edge, port_t in_port, uint8_t in_pin, action_t action, port_t out_port,
                                 uint8_t out_pin)
{
    spy.edge     = edge;
    spy.port     = in_port;
    spy.pin      = in_pin;
    spy.action   = action;
    spy.out_port = out_port;
    spy.out_pin  = out_pin;

    return record(CALL_IRQ_BIND, mcu);
}

mcu_status_t mcuco_gpio_irq_unbind(mcuco_t *mcu, port_t port, uint8_t pin)
{
    spy.port = port;
    spy.pin  = pin;

    return record(CALL_IRQ_UNBIND, mcu);
}
