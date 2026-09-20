#ifndef MCUCO_H
#define MCUCO_H

#include <stdint.h>

/* Codes 1-10 are the firmware's status_t, sent verbatim as a NACK's reason
 * byte: fixed on the wire, append-only. Host-side codes start at 0x80 so they
 * can never collide with one. */
typedef enum
{
    STATUS_OK = 0,

    STATUS_ERR               = 1,
    STATUS_ERR_INVALID_ARG   = 2,
    STATUS_ERR_INVALID_PIN   = 3,
    STATUS_ERR_INVALID_STATE = 4,
    STATUS_ERR_NOT_INIT      = 5,
    STATUS_ERR_BUSY          = 6,
    STATUS_ERR_TIMEOUT       = 7,
    STATUS_ERR_UNSUPPORTED   = 8,
    STATUS_ERR_EMPTY         = 9,
    STATUS_ERR_FULL          = 10,

    /* Distinct from STATUS_ERR_INVALID_ARG so a local range check is
     * distinguishable from the MCU's own refusal. */
    STATUS_ERR_ARG = 0x80,

    STATUS_ERR_IO = 0x81,

    /* The MCU answers a bad frame with silence, so this cannot be told apart
     * from an absent MCU. */
    STATUS_ERR_NO_RESPONSE = 0x82,

    STATUS_ERR_BAD_FRAME = 0x83,
    STATUS_ERR_NOT_OPEN  = 0x84,
} mcu_status_t;

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

#define MCUCO_TIMEOUT_DEFAULT_MS 1000

/* Not thread-safe: the protocol allows one command in flight. */
typedef struct mcuco mcuco_t;

const char *mcuco_strerror(mcu_status_t status);

/* Returns NULL on failure with errno set, as fopen does. */
mcuco_t *mcuco_open(const char *device_path, int timeout_ms);

/* Closes the port and frees the handle. A NULL mcu is ignored. */
void mcuco_close(mcuco_t *mcu);

/* Confirms mcu-co is on the other end of the port, rather than some other
 * device that happened to answer. Fails with STATUS_ERR_BAD_FRAME if the reply
 * is well formed but carries the wrong magic. */
mcu_status_t mcuco_probe(mcuco_t *mcu);

/* Sets a pin's direction. */
mcu_status_t mcuco_gpio_cfg(mcuco_t *mcu, dir_t dir, port_t port, uint8_t pin);

/* Drives an output pin. */
mcu_status_t mcuco_gpio_set(mcuco_t *mcu, level_t level, port_t port, uint8_t pin);

/* Flips an output pin and reports the level it ended up at, saving the round
 * trip a set-then-get would cost. `level` is untouched unless STATUS_OK. */
mcu_status_t mcuco_gpio_toggle(mcuco_t *mcu, port_t port, uint8_t pin, level_t *level);

/* Reads an input pin. `level` is untouched unless STATUS_OK is returned. */
mcu_status_t mcuco_gpio_get(mcuco_t *mcu, port_t port, uint8_t pin, level_t *level);

/* Arms the trigger only. Set direction with mcuco_gpio_cfg first; attach an
 * action with mcuco_gpio_irq_bind after. EDGE_OFF disarms and clears bindings. */
mcu_status_t mcuco_gpio_irq_cfg(mcuco_t *mcu, edge_t edge, port_t port, uint8_t pin);

/* Runs entirely on the MCU once this returns. `edge` must match the armed edge
 * exactly. Rebinding without unbinding first fails with STATUS_ERR_BUSY. */
mcu_status_t mcuco_gpio_irq_bind(mcuco_t *mcu, edge_t edge, port_t in_port, uint8_t in_pin, action_t action, port_t out_port,
                                 uint8_t out_pin);

/* Drops the action but leaves the trigger armed. */
mcu_status_t mcuco_gpio_irq_unbind(mcuco_t *mcu, port_t port, uint8_t pin);

/* All four pins in a group share its frequency. Reconfiguring a live group
 * fails with STATUS_ERR_BUSY and changes nothing; release it first. */
mcu_status_t mcuco_pwm_group_cfg(mcuco_t *mcu, uint32_t freq_hz, uint8_t group);

/* The achieved frequency, which integer prescaler division can make differ
 * from the one requested. Untouched unless STATUS_OK is returned. */
mcu_status_t mcuco_pwm_group_get(mcuco_t *mcu, uint8_t group, uint32_t *achieved_hz);

/* Stops the counter and frees all four of the group's channels. */
mcu_status_t mcuco_pwm_group_release(mcuco_t *mcu, uint8_t group);

/* Claims a pin for PWM. It comes up silent at 0.0% until mcuco_pwm_channel_set. */
mcu_status_t mcuco_pwm_channel_cfg(mcuco_t *mcu, polarity_t polarity, port_t port, uint8_t pin);

/* Duty is tenths of a percent. Silence an output with a duty of 0, not a group
 * release: stopping a counter freezes the pin at whatever level it held. */
mcu_status_t mcuco_pwm_channel_set(mcuco_t *mcu, uint16_t duty, port_t port, uint8_t pin);

/* Reads back a claimed pin's duty. Untouched unless STATUS_OK is returned. */
mcu_status_t mcuco_pwm_channel_get(mcuco_t *mcu, port_t port, uint8_t pin, uint16_t *duty);

/* Frees one pin, leaving the rest of the group running. */
mcu_status_t mcuco_pwm_channel_release(mcuco_t *mcu, port_t port, uint8_t pin);

#endif /* MCUCO_H */
