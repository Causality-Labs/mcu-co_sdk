#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "mcuco_args.h"
#include "mcuco_command.h"

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

    case WORD_NONE:
        break;
    }

    return -1;
}

int args_parse_mcuco(int word_count, char **words, mcuco_args_t *mcuco_args)
{
    if (word_count <= 0)
    {
        return -EINVAL;
    }

    if (word_count < FIRST_VALUE_WORD)
    {
        return -1;
    }

    const command_t *command = mcuco_find_command(words[SUBSYSTEM_WORD], words[VERB_WORD]);
    if (command == NULL)
    {
        return -1;
    }

    mcuco_args->command = command;

    if (word_count != FIRST_VALUE_WORD + command->value_count)
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
