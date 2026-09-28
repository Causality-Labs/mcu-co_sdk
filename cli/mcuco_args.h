#ifndef MCUCO_ARGS_H
#define MCUCO_ARGS_H

#include <stdint.h>

#include "mcuco.h"

typedef enum
{
    MCU = 0,
    GPIO,
    IRQ,
    TIMER,
    PWM,
    NUM_OF_SUBSYSTEMS
} subsytem_t;

typedef enum
{
    CONFIG = 0,
    SET,
    GET,
    TOGGLE,
    BIND,
    UNBIND,
    RELEASE,
    PROBE,
    RESET,
    NUM_OF_VERBS
} verb_t;

typedef struct
{
    verb_t verb;
} mcu_system_t;

typedef struct
{
    verb_t verb;
    dir_t direction;
    level_t level;
    port_t port;
    uint8_t pin;
} gpio_system_t;

typedef struct
{
    verb_t verb;
    edge_t edge;
    port_t port;
    uint8_t pin;
    action_t action;
    port_t out_port;
    uint8_t out_pin;
} irq_system_t;

typedef struct
{
    verb_t verb;
    uint32_t frequency_hz;
    uint8_t timer;
} timer_system_t;

typedef struct
{
    verb_t verb;
    polarity_t polarity;
    uint16_t duty_tenths;
    port_t port;
    uint8_t pin;
} pwm_system_t;

typedef struct
{
    subsytem_t subsytem;
    union
    {
        gpio_system_t gpio;
        irq_system_t irq;
        timer_system_t timer;
        pwm_system_t pwm;
        mcu_system_t mcu;
    } system;
} mcuco_args_t;

int args_parse_mcuco(int word_count, char **words, mcuco_args_t *mcuco_args);

#endif /* MCUCO_ARGS_H */
