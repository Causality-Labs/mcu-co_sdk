#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "mcuco_args.h"

// clang-format off
static const char *const subsystem_list[NUM_OF_SUBSYSTEMS] = {
    [MCU]   = "mcu",
    [GPIO]  = "gpio",
    [IRQ]   = "irq",
    [TIMER] = "timer",
    [PWM]   = "pwm",
};
// clang-format on

// clang-format off
static const char *const verb_list[NUM_OF_VERBS] = {
    [CONFIG]  = "cfg",
    [SET]     = "set",
    [GET]     = "get",
    [TOGGLE]  = "toggle",
    [BIND]    = "bind",
    [UNBIND]  = "unbind",
    [RELEASE] = "release",
    [PROBE] = "probe",
    [RESET] = "reset"
};
// clang-format on

enum
{
    SUBSYSTEM_WORD = 0,
    SUBSYSTEM_WORD_COUNT,
};

/* Word positions once the subsystem word is stripped: the verb, then its values in order. */
enum
{
    VERB_WORD = 0,
};

enum
{
    MCU_PROBE_WORD_COUNT = VERB_WORD + 1,
};

enum
{
    MCU_RESET_WORD_COUNT = VERB_WORD + 1,
};

enum
{
    GPIO_CFG_DIRECTION_WORD = VERB_WORD + 1,
    GPIO_CFG_PORT_WORD,
    GPIO_CFG_PIN_WORD,
    GPIO_CFG_WORD_COUNT,
};

enum
{
    GPIO_SET_LEVEL_WORD = VERB_WORD + 1,
    GPIO_SET_PORT_WORD,
    GPIO_SET_PIN_WORD,
    GPIO_SET_WORD_COUNT,
};

enum
{
    GPIO_GET_PORT_WORD = VERB_WORD + 1,
    GPIO_GET_PIN_WORD,
    GPIO_GET_WORD_COUNT,
};

enum
{
    GPIO_TOGGLE_PORT_WORD = VERB_WORD + 1,
    GPIO_TOGGLE_PIN_WORD,
    GPIO_TOGGLE_WORD_COUNT,
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

static int assign_mcu_system(mcu_system_t *mcu, int word_count, char **words)
{
    if (word_count <= VERB_WORD)
    {
        return -1;
    }

    if (strcmp(words[VERB_WORD], verb_list[PROBE]) == 0)
    {
        if (word_count != MCU_PROBE_WORD_COUNT)
        {
            return -1;
        }

        mcu->verb = PROBE;
        return 0;
    }

    if (strcmp(words[VERB_WORD], verb_list[RESET]) == 0)
    {
        if (word_count != MCU_RESET_WORD_COUNT)
        {
            return -1;
        }

        mcu->verb = RESET;
        return 0;
    }

    return -1;
}

static int assign_gpio_system(gpio_system_t *gpio, int word_count, char **words)
{
    if (word_count <= VERB_WORD)
    {
        return -1;
    }

    if (strcmp(words[VERB_WORD], verb_list[CONFIG]) == 0)
    {
        if (word_count != GPIO_CFG_WORD_COUNT)
        {
            return -1;
        }

        if (parse_direction(words[GPIO_CFG_DIRECTION_WORD], &gpio->direction) != 0)
        {
            return -1;
        }

        if (parse_port(words[GPIO_CFG_PORT_WORD], &gpio->port) != 0)
        {
            return -1;
        }

        if (parse_pin(words[GPIO_CFG_PIN_WORD], &gpio->pin) != 0)
        {
            return -1;
        }

        gpio->verb = CONFIG;
        return 0;
    }

    if (strcmp(words[VERB_WORD], verb_list[SET]) == 0)
    {
        if (word_count != GPIO_SET_WORD_COUNT)
        {
            return -1;
        }

        if (parse_level(words[GPIO_SET_LEVEL_WORD], &gpio->level) != 0)
        {
            return -1;
        }

        if (parse_port(words[GPIO_SET_PORT_WORD], &gpio->port) != 0)
        {
            return -1;
        }

        if (parse_pin(words[GPIO_SET_PIN_WORD], &gpio->pin) != 0)
        {
            return -1;
        }

        gpio->verb = SET;
        return 0;
    }

    if (strcmp(words[VERB_WORD], verb_list[GET]) == 0)
    {
        if (word_count != GPIO_GET_WORD_COUNT)
        {
            return -1;
        }

        if (parse_port(words[GPIO_GET_PORT_WORD], &gpio->port) != 0)
        {
            return -1;
        }

        if (parse_pin(words[GPIO_GET_PIN_WORD], &gpio->pin) != 0)
        {
            return -1;
        }

        gpio->verb = GET;
        return 0;
    }

    if (strcmp(words[VERB_WORD], verb_list[TOGGLE]) == 0)
    {
        if (word_count != GPIO_TOGGLE_WORD_COUNT)
        {
            return -1;
        }

        if (parse_port(words[GPIO_TOGGLE_PORT_WORD], &gpio->port) != 0)
        {
            return -1;
        }

        if (parse_pin(words[GPIO_TOGGLE_PIN_WORD], &gpio->pin) != 0)
        {
            return -1;
        }

        gpio->verb = TOGGLE;
        return 0;
    }

    return -1;
}

static int assign_subsystem(mcuco_args_t *mcuco_args, int word_count, char **words)
{
    switch (mcuco_args->subsytem)
    {
    case MCU:
        return assign_mcu_system(&mcuco_args->system.mcu, word_count, words);

    case GPIO:
        return assign_gpio_system(&mcuco_args->system.gpio, word_count, words);

    case IRQ:
    case TIMER:
    case PWM:
    default:
        return -1;
    }
}

int args_parse_mcuco(int word_count, char **words, mcuco_args_t *mcuco_args)
{
    if (word_count <= SUBSYSTEM_WORD)
    {
        return -EINVAL;
    }

    size_t subsystem_count = sizeof(subsystem_list) / sizeof(subsystem_list[0]);

    mcuco_args->subsytem = NUM_OF_SUBSYSTEMS;

    for (size_t subsystem_index = 0; subsystem_index < subsystem_count; subsystem_index++)
    {
        if (strcmp(words[SUBSYSTEM_WORD], subsystem_list[subsystem_index]) == 0)
        {
            mcuco_args->subsytem = (subsytem_t)subsystem_index;
        }
    }

    if (mcuco_args->subsytem == NUM_OF_SUBSYSTEMS)
    {
        return -1;
    }

    if (assign_subsystem(mcuco_args, word_count - SUBSYSTEM_WORD_COUNT, &words[SUBSYSTEM_WORD_COUNT]) != 0)
    {
        return -1;
    }
    return 0;
}
