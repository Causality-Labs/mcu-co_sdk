#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "mcuco_command.h"

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

/* The timer commands parse, but nothing calls mcuco_pwm_group_* for them yet. */
static mcu_status_t run_not_wired(mcuco_t *mcu, const mcuco_args_t *mcuco_args)
{
    (void)mcu;
    (void)mcuco_args;

    return STATUS_ERR_UNSUPPORTED;
}

// clang-format off
static const command_t commands[] = {
    {"mcu",   "probe",   0, {WORD_NONE},                            run_probe,       "",                            "check the link"},
    {"mcu",   "reset",   0, {WORD_NONE},                            run_reset,       "",                            "reboot the MCU"},

    {"gpio",  "cfg",     3, {WORD_DIRECTION, WORD_PORT, WORD_PIN},  run_gpio_cfg,    "<input|output> <pin>", "set a pin's direction"},
    {"gpio",  "set",     3, {WORD_LEVEL, WORD_PORT, WORD_PIN},      run_gpio_set,    "<low|high> <pin>",     "drive an output pin"},
    {"gpio",  "get",     2, {WORD_PORT, WORD_PIN},                  run_gpio_get,    "<pin>",                "-> low | high"},
    {"gpio",  "toggle",  2, {WORD_PORT, WORD_PIN},                  run_gpio_toggle, "<pin>",                "-> the level after the flip"},

    {"timer", "cfg",     2, {WORD_FREQUENCY, WORD_TIMER},           run_not_wired,   "<1-1000000> <0-2>",           "Hz, then which timer"},
    {"timer", "get",     1, {WORD_TIMER},                           run_not_wired,   "<0-2>",                       "-> achieved Hz"},
    {"timer", "release", 1, {WORD_TIMER},                           run_not_wired,   "<0-2>",                       "stop it, freezing its pins"},

    {"pwm",   "cfg",     3, {WORD_POLARITY, WORD_PORT, WORD_PIN},   run_not_wired,   "<polarity> <pin>",     "claim a pin, silent at 0%"},
    {"pwm",   "set",     3, {WORD_DUTY, WORD_PORT, WORD_PIN},       run_not_wired,   "<0-100> <pin>",        "percent, then the pin"},
    {"pwm",   "get",     2, {WORD_PORT, WORD_PIN},                  run_not_wired,   "<pin>",                "-> percent, one decimal"},
    {"pwm",   "release", 2, {WORD_PORT, WORD_PIN},                  run_not_wired,   "<pin>",                "free one pin"},

    {"irq",   "cfg",     3, {WORD_EDGE, WORD_PORT, WORD_PIN},       run_not_wired,   "<edge> <pin>",         "arm or disarm a trigger"},
    {"irq",   "bind",    6, {WORD_BIND_EDGE, WORD_PORT, WORD_PIN, WORD_ACTION, WORD_OUT_PORT, WORD_OUT_PIN}, run_not_wired,   "<edge> <pin> <action> <pin>", "drive one pin from another"},
    {"irq",   "unbind",  2, {WORD_PORT, WORD_PIN},                  run_not_wired,   "<pin>",                "drop the action, stay armed"},
};
// clang-format on

#define COMMAND_COUNT (sizeof(commands) / sizeof(commands[0]))

const command_t *mcuco_find_command(const char *subsystem, const char *verb)
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
