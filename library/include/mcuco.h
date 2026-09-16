#ifndef MCUCO_H
#define MCUCO_H

#include <stdint.h>

#include "status.h"

/* One call per command in mcu-co_Protocol.md. Arguments are value-first -
 * mcuco_gpio_set(mcu, level, port, pin) - matching CLI token order and payload
 * byte order. */

/* Field encodings, fixed on the wire. */

typedef enum
{
    PORT_A = 0,
    PORT_B = 1,
    PORT_C = 2,
    PORT_D = 3,
    PORT_E = 4,
    PORT_F = 5,
    PORT_G = 6,
} port_t;

typedef enum
{
    DIR_INPUT  = 0,
    DIR_OUTPUT = 1,
} dir_t;

typedef enum
{
    LEVEL_LOW  = 0,
    LEVEL_HIGH = 1,
} level_t;

/* EDGE_OFF disarms the pin and clears its binding; never valid for a bind. */
typedef enum
{
    EDGE_OFF     = 0,
    EDGE_RISING  = 1,
    EDGE_FALLING = 2,
    EDGE_BOTH    = 3,
} edge_t;

/* ACTION_TOGGLE is edge-agnostic: the way to mirror an input on an output. */
typedef enum
{
    ACTION_LOW    = 0,
    ACTION_HIGH   = 1,
    ACTION_TOGGLE = 2,
} action_t;

/* Level held during the active part of a PWM period. */
typedef enum
{
    POL_ACTIVE_HIGH = 0,
    POL_ACTIVE_LOW  = 1,
} polarity_t;

#define PIN_MAX   15U
#define GROUP_MAX 2U       /* three groups: TIM2, TIM3, TIM4 */
#define DUTY_MAX  1000U    /* tenths of a percent, so 1000 is 100.0% */
#define FREQ_MIN  1U       /* Hz. 0 is not shorthand for teardown */
#define FREQ_MAX  1000000U /* Hz */

#define TIMEOUT_DEFAULT_MS 1000

/* Not thread-safe: the protocol allows one command in flight. */
typedef struct mcuco mcuco_t;

/* Returns NULL on failure with errno set, as fopen does. */
mcuco_t *mcuco_open(const char *device_path, int timeout_ms);

/* Closes the port and frees the handle. A NULL mcu is ignored. */
void mcuco_close(mcuco_t *mcu);

#endif /* MCUCO_H */
