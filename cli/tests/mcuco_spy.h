#ifndef MCUCO_SPY_H
#define MCUCO_SPY_H

#include <stdint.h>

#include "mcuco.h"

/* A spy standing in for libmcuco, so a test can check which call mcuco_command
 * made and with what, without a serial port, a pty or a frame in sight.
 *
 * It is linked in place of the real library, not alongside it: cli_tests builds
 * mcuco_command.c against these definitions and never links libmcuco. */

typedef enum
{
    CALL_NONE = 0,
    CALL_PROBE,
    CALL_RESET,
    CALL_GPIO_CFG,
    CALL_GPIO_SET,
    CALL_GPIO_GET,
    CALL_GPIO_TOGGLE,
    CALL_PWM_GROUP_CFG,
    CALL_PWM_GROUP_GET,
    CALL_PWM_GROUP_RELEASE,
    CALL_PWM_CHANNEL_CFG,
    CALL_PWM_CHANNEL_SET,
    CALL_PWM_CHANNEL_GET,
    CALL_PWM_CHANNEL_RELEASE,
    CALL_IRQ_CFG,
    CALL_IRQ_BIND,
    CALL_IRQ_UNBIND,
} mcuco_call_t;

typedef struct
{
    /* Total across every function, so that a test can assert exactly one
     * library call was made - or none at all, for a refused command. */
    int calls;
    mcuco_call_t last_call;

    /* The handle it was handed, to prove it was passed through. */
    const mcuco_t *handle;

    /* What it was handed. Only the fields the last call takes are meaningful. */
    port_t port;
    uint8_t pin;
    dir_t direction;
    level_t level;
    uint32_t frequency_hz;
    uint8_t group;
    polarity_t polarity;
    uint16_t duty;
    edge_t edge;
    action_t action;
    port_t out_port;
    uint8_t out_pin;

    /* What it hands back. Set these before the call. */
    mcu_status_t next_status;
    level_t next_level;
    uint32_t next_frequency_hz;
    uint16_t next_duty;
} mcuco_spy_t;

/* Forgets every recorded call and sets the canned answers back to STATUS_OK and
 * LEVEL_LOW. Belongs in setup(). */
void mcuco_spy_reset(void);

/* The recording. Lives for the life of the process. */
mcuco_spy_t *mcuco_spy(void);

/* A non-NULL handle to hand to the code under test. mcuco_t is opaque, so a
 * test cannot make one of its own; the spy only records that it arrived. */
mcuco_t *mcuco_spy_handle(void);

#endif /* MCUCO_SPY_H */
