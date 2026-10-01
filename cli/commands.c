#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "commands.h"

/* The subsystem and verb name the command; its values follow in row order. */
enum
{
    SUBSYSTEM_WORD = 0,
    VERB_WORD,
    FIRST_VALUE_WORD,
};

static int parse_direction(const char *word, dir_t *direction)
{
    if (strcmp(word, "input") == 0)
    {
        *direction = DIR_INPUT;
        return 0;
    }

    if (strcmp(word, "output") == 0)
    {
        *direction = DIR_OUTPUT;
        return 0;
    }

    return -1;
}

static int parse_level(const char *word, level_t *level)
{
    if (strcmp(word, "low") == 0)
    {
        *level = LEVEL_LOW;
        return 0;
    }

    if (strcmp(word, "high") == 0)
    {
        *level = LEVEL_HIGH;
        return 0;
    }

    return -1;
}

static int parse_port(const char *word, port_t *port)
{
    if (word[0] < 'A' || word[0] > 'G' || word[1] != '\0')
    {
        return -1;
    }

    *port = (port_t)(word[0] - 'A');
    return 0;
}

static int parse_pin(const char *word, uint8_t *pin)
{
    /* strtoul would accept "-1" and wrap it, so require a digit first. */
    if (word[0] < '0' || word[0] > '9')
    {
        return -1;
    }

    char *end           = NULL;
    unsigned long value = strtoul(word, &end, 10);
    if (*end != '\0' || value > PIN_MAX)
    {
        return -1;
    }

    *pin = (uint8_t)value;
    return 0;
}

static int parse_timer(const char *word, uint8_t *timer)
{
    if (word[0] < '0' || word[0] > '9')
    {
        return -1;
    }

    char *end           = NULL;
    unsigned long value = strtoul(word, &end, 10);
    if (*end != '\0' || value > GROUP_MAX)
    {
        return -1;
    }

    *timer = (uint8_t)value;
    return 0;
}

static int parse_frequency(const char *word, uint32_t *frequency_hz)
{
    char *end           = NULL;
    unsigned long value = strtoul(word, &end, 10);
    if (*end != '\0' || value < FREQ_MIN || value > FREQ_MAX)
    {
        return -1;
    }

    *frequency_hz = (uint32_t)value;
    return 0;
}

static int parse_polarity(const char *word, polarity_t *polarity)
{
    if (strcmp(word, "active-high") == 0)
    {
        *polarity = POL_ACTIVE_HIGH;
        return 0;
    }

    if (strcmp(word, "active-low") == 0)
    {
        *polarity = POL_ACTIVE_LOW;
        return 0;
    }

    return -1;
}

static int parse_duty(const char *word, uint16_t *duty_tenths)
{
    if (word[0] < '0' || word[0] > '9')
    {
        return -1;
    }

    char *end             = NULL;
    unsigned long percent = strtoul(word, &end, 10);
    if (*end != '\0' || percent > DUTY_MAX / 10U)
    {
        return -1;
    }

    *duty_tenths = (uint16_t)(percent * 10U);
    return 0;
}

static int parse_edge(const char *word, edge_t *edge)
{
    if (strcmp(word, "off") == 0)
    {
        *edge = EDGE_OFF;
        return 0;
    }

    if (strcmp(word, "rising") == 0)
    {
        *edge = EDGE_RISING;
        return 0;
    }

    if (strcmp(word, "falling") == 0)
    {
        *edge = EDGE_FALLING;
        return 0;
    }

    if (strcmp(word, "both") == 0)
    {
        *edge = EDGE_BOTH;
        return 0;
    }

    return -1;
}

static int parse_bind_edge(const char *word, edge_t *edge)
{
    if (strcmp(word, "off") == 0)
    {
        return -1;
    }

    return parse_edge(word, edge);
}

static int parse_action(const char *word, action_t *action)
{
    if (strcmp(word, "low") == 0)
    {
        *action = ACTION_LOW;
        return 0;
    }

    if (strcmp(word, "high") == 0)
    {
        *action = ACTION_HIGH;
        return 0;
    }

    if (strcmp(word, "toggle") == 0)
    {
        *action = ACTION_TOGGLE;
        return 0;
    }

    return -1;
}

static int parse_value(word_kind_t kind, const char *word, mcuco_args_t *mcuco_args)
{
    switch (kind)
    {
    case WORD_DIRECTION:
        return parse_direction(word, &mcuco_args->direction);

    case WORD_LEVEL:
        return parse_level(word, &mcuco_args->level);

    case WORD_PORT:
        return parse_port(word, &mcuco_args->port);

    case WORD_PIN:
        return parse_pin(word, &mcuco_args->pin);

    case WORD_FREQUENCY:
        return parse_frequency(word, &mcuco_args->frequency_hz);

    case WORD_TIMER:
        return parse_timer(word, &mcuco_args->timer);

    case WORD_POLARITY:
        return parse_polarity(word, &mcuco_args->polarity);

    case WORD_DUTY:
        return parse_duty(word, &mcuco_args->duty_tenths);

    case WORD_EDGE:
        return parse_edge(word, &mcuco_args->edge);

    case WORD_BIND_EDGE:
        return parse_bind_edge(word, &mcuco_args->edge);

    case WORD_ACTION:
        return parse_action(word, &mcuco_args->action);

    case WORD_OUT_PORT:
        return parse_port(word, &mcuco_args->out_port);

    case WORD_OUT_PIN:
        return parse_pin(word, &mcuco_args->out_pin);

    case WORD_NONE:
        break;
    }

    return -1;
}

static void print_level(level_t level)
{
    printf("%s\n", (level == LEVEL_HIGH) ? "high" : "low");
}

static mcu_status_t run_probe(mcuco_t *mcu, const mcuco_args_t *mcuco_args)
{
    (void)mcuco_args;

    return mcuco_probe(mcu);
}

static mcu_status_t run_reset(mcuco_t *mcu, const mcuco_args_t *mcuco_args)
{
    (void)mcuco_args;

    return mcuco_reset(mcu);
}

static mcu_status_t run_gpio_cfg(mcuco_t *mcu, const mcuco_args_t *mcuco_args)
{
    return mcuco_gpio_cfg(mcu, mcuco_args->direction, mcuco_args->port, mcuco_args->pin);
}

static mcu_status_t run_gpio_set(mcuco_t *mcu, const mcuco_args_t *mcuco_args)
{
    return mcuco_gpio_set(mcu, mcuco_args->level, mcuco_args->port, mcuco_args->pin);
}

static mcu_status_t run_gpio_get(mcuco_t *mcu, const mcuco_args_t *mcuco_args)
{
    level_t level = LEVEL_LOW;

    mcu_status_t status = mcuco_gpio_get(mcu, mcuco_args->port, mcuco_args->pin, &level);
    if (status == STATUS_OK)
    {
        print_level(level);
    }

    return status;
}

static mcu_status_t run_gpio_toggle(mcuco_t *mcu, const mcuco_args_t *mcuco_args)
{
    level_t level = LEVEL_LOW;

    mcu_status_t status = mcuco_gpio_toggle(mcu, mcuco_args->port, mcuco_args->pin, &level);
    if (status == STATUS_OK)
    {
        print_level(level);
    }

    return status;
}

static mcu_status_t run_timer_cfg(mcuco_t *mcu, const mcuco_args_t *mcuco_args)
{
    return mcuco_pwm_group_cfg(mcu, mcuco_args->frequency_hz, mcuco_args->timer);
}

static mcu_status_t run_timer_release(mcuco_t *mcu, const mcuco_args_t *mcuco_args)
{
    return mcuco_pwm_group_release(mcu, mcuco_args->timer);
}

static mcu_status_t run_timer_get(mcuco_t *mcu, const mcuco_args_t *mcuco_args)
{
    uint32_t achieved_hz = 0;

    mcu_status_t status = mcuco_pwm_group_get(mcu, mcuco_args->timer, &achieved_hz);
    if (status == STATUS_OK)
    {
        printf("%u\n", achieved_hz);
    }

    return status;
}

static mcu_status_t run_pwm_cfg(mcuco_t *mcu, const mcuco_args_t *mcuco_args)
{
    return mcuco_pwm_channel_cfg(mcu, mcuco_args->polarity, mcuco_args->port, mcuco_args->pin);
}

static mcu_status_t run_pwm_set(mcuco_t *mcu, const mcuco_args_t *mcuco_args)
{
    return mcuco_pwm_channel_set(mcu, mcuco_args->duty_tenths, mcuco_args->port, mcuco_args->pin);
}

static mcu_status_t run_pwm_release(mcuco_t *mcu, const mcuco_args_t *mcuco_args)
{
    return mcuco_pwm_channel_release(mcu, mcuco_args->port, mcuco_args->pin);
}

static mcu_status_t run_pwm_get(mcuco_t *mcu, const mcuco_args_t *mcuco_args)
{
    uint16_t duty_tenths = 0;

    mcu_status_t status = mcuco_pwm_channel_get(mcu, mcuco_args->port, mcuco_args->pin, &duty_tenths);
    if (status == STATUS_OK)
    {
        printf("%u.%u\n", duty_tenths / 10U, duty_tenths % 10U);
    }

    return status;
}

static mcu_status_t run_irq_cfg(mcuco_t *mcu, const mcuco_args_t *mcuco_args)
{
    return mcuco_gpio_irq_cfg(mcu, mcuco_args->edge, mcuco_args->port, mcuco_args->pin);
}

static mcu_status_t run_irq_bind(mcuco_t *mcu, const mcuco_args_t *mcuco_args)
{
    return mcuco_gpio_irq_bind(mcu, mcuco_args->edge, mcuco_args->port, mcuco_args->pin, mcuco_args->action,
                               mcuco_args->out_port, mcuco_args->out_pin);
}

static mcu_status_t run_irq_unbind(mcuco_t *mcu, const mcuco_args_t *mcuco_args)
{
    return mcuco_gpio_irq_unbind(mcu, mcuco_args->port, mcuco_args->pin);
}

// clang-format off
static const command_t commands[] = {
    {"mcu",   "probe",   0, {WORD_NONE},                            run_probe,       "",                            "check the link"},
    {"mcu",   "reset",   0, {WORD_NONE},                            run_reset,       "",                            "reboot the MCU"},

    {"gpio",  "cfg",     3, {WORD_DIRECTION, WORD_PORT, WORD_PIN},  run_gpio_cfg,    "<input|output> <pin>", "set a pin's direction"},
    {"gpio",  "set",     3, {WORD_LEVEL, WORD_PORT, WORD_PIN},      run_gpio_set,    "<low|high> <pin>",     "drive an output pin"},
    {"gpio",  "get",     2, {WORD_PORT, WORD_PIN},                  run_gpio_get,    "<pin>",                "-> low | high"},
    {"gpio",  "toggle",  2, {WORD_PORT, WORD_PIN},                  run_gpio_toggle, "<pin>",                "-> the level after the flip"},

    {"timer", "cfg",     2, {WORD_FREQUENCY, WORD_TIMER},           run_timer_cfg,   "<1-1000000> <0-2>",           "Hz, then which timer"},
    {"timer", "get",     1, {WORD_TIMER},                           run_timer_get,   "<0-2>",                       "-> achieved Hz"},
    {"timer", "release", 1, {WORD_TIMER},                           run_timer_release,   "<0-2>",                       "stop it, freezing its pins"},

    {"pwm",   "cfg",     3, {WORD_POLARITY, WORD_PORT, WORD_PIN},   run_pwm_cfg,     "<polarity> <pin>",     "claim a pin, silent at 0%"},
    {"pwm",   "set",     3, {WORD_DUTY, WORD_PORT, WORD_PIN},       run_pwm_set,     "<0-100> <pin>",        "percent, then the pin"},
    {"pwm",   "get",     2, {WORD_PORT, WORD_PIN},                  run_pwm_get,     "<pin>",                "-> percent, one decimal"},
    {"pwm",   "release", 2, {WORD_PORT, WORD_PIN},                  run_pwm_release,   "<pin>",                "free one pin"},

    {"irq",   "cfg",     3, {WORD_EDGE, WORD_PORT, WORD_PIN},       run_irq_cfg,     "<edge> <pin>",         "arm or disarm a trigger"},
    {"irq",   "bind",    6, {WORD_BIND_EDGE, WORD_PORT, WORD_PIN, WORD_ACTION, WORD_OUT_PORT, WORD_OUT_PIN}, run_irq_bind,   "<edge> <pin> <action> <pin>", "drive one pin from another"},
    {"irq",   "unbind",  2, {WORD_PORT, WORD_PIN},                  run_irq_unbind,   "<pin>",                "drop the action, stay armed"},
};
// clang-format on

#define COMMAND_COUNT (sizeof(commands) / sizeof(commands[0]))

static const command_t *mcuco_find_command(const char *subsystem, const char *verb)
{
    for (size_t row = 0; row < COMMAND_COUNT; row++)
    {
        if (strcmp(commands[row].subsystem, subsystem) == 0 && strcmp(commands[row].verb, verb) == 0)
        {
            return &commands[row];
        }
    }

    return NULL;
}

int args_parse_mcuco(int word_count, char **words, mcuco_args_t *mcuco_args)
{
    if (word_count < FIRST_VALUE_WORD)
    {
        return -1;
    }

    const command_t *command = mcuco_find_command(words[SUBSYSTEM_WORD], words[VERB_WORD]);
    if (command == NULL)
    {
        return -1;
    }

    mcuco_args->command     = command;
    int expected_word_count = FIRST_VALUE_WORD + command->value_count;
    if (word_count != expected_word_count)
    {
        return -1;
    }

    for (int value_index = 0; value_index < command->value_count; value_index++)
    {
        if (parse_value(command->values[value_index], words[FIRST_VALUE_WORD + value_index], mcuco_args) != 0)
        {
            return -1;
        }
    }

    return 0;
}

mcu_status_t mcuco_run_command(mcuco_t *mcu, const mcuco_args_t *mcuco_args)
{
    if (mcuco_args->command == NULL)
    {
        return STATUS_ERR_ARG;
    }

    return mcuco_args->command->run(mcu, mcuco_args);
}

void mcuco_print_commands(FILE *stream)
{
    for (size_t row = 0; row < COMMAND_COUNT; row++)
    {
        bool starts_subsystem = (row == 0) || (strcmp(commands[row].subsystem, commands[row - 1].subsystem) != 0);

        if (starts_subsystem && row > 0)
        {
            fprintf(stream, "\n");
        }

        fprintf(stream, "  %-6s %-8s %-28s %s\n", starts_subsystem ? commands[row].subsystem : "", commands[row].verb,
                commands[row].usage, commands[row].summary);
    }
}
