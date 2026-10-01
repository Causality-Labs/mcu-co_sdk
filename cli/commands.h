#ifndef COMMANDS_H
#define COMMANDS_H

#include <stdint.h>
#include <stdio.h>

#include "mcuco.h"

/* What a command's value words are, in the order they are typed. WORD_NONE
 * fills the slots a row does not use. */
typedef enum
{
    WORD_NONE = 0,
    WORD_DIRECTION,
    WORD_LEVEL,
    WORD_PORT,
    WORD_PIN,
    WORD_FREQUENCY,
    WORD_TIMER,
    WORD_POLARITY,
    WORD_DUTY,
    WORD_EDGE,
    WORD_BIND_EDGE,
    WORD_ACTION,
    WORD_OUT_PORT,
    WORD_OUT_PIN,
} word_kind_t;

#define MAX_VALUE_WORDS 6

typedef struct mcuco_args mcuco_args_t;

typedef struct
{
    const char *subsystem;
    const char *verb;
    int value_count;
    word_kind_t values[MAX_VALUE_WORDS];
    mcu_status_t (*run)(mcuco_t *mcu, const mcuco_args_t *mcuco_args);
    const char *usage;
    const char *summary;
} command_t;

struct mcuco_args
{
    const command_t *command;
    dir_t direction;
    level_t level;
    port_t port;
    uint8_t pin;
    uint32_t frequency_hz;
    uint8_t timer;
    polarity_t polarity;
    uint16_t duty_tenths;
    edge_t edge;
    action_t action;
    port_t out_port;
    uint8_t out_pin;
};

int args_parse_mcuco(int word_count, char **words, mcuco_args_t *mcuco_args);
// const command_t *mcuco_find_command(const char *subsystem, const char *verb);
mcu_status_t mcuco_run_command(mcuco_t *mcu, const mcuco_args_t *mcuco_args);
void mcuco_print_commands(FILE *stream);

#endif /* COMMANDS_H */
