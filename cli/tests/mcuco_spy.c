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
