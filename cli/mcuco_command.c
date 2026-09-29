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
