#include <stdio.h>
#include <string.h>

#define EXIT_OK    0
#define EXIT_USAGE 2

#define ARRAY_COUNT(array) (sizeof(array) / sizeof((array)[0]))

typedef struct
{
    const char *name;
    const char *flags;
    const char *summary;
} verb_t;

/* Nested rather than one flat table of all sixteen commands: a verb cannot
 * then name a subsystem that does not exist, and the subsystem list needs no
 * de-duplicating. */
typedef struct
{
    const char *name;
    const char *summary;
    const verb_t *verbs;
    size_t verb_count;
} subsystem_t;

static const verb_t gpio_verbs[] = {
    {"cfg", "-p <pin> -d <input|output>", "set a pin's direction"},
    {"set", "-p <pin> -l <low|high>", "drive an output pin"},
    {"get", "-p <pin>", "read an input pin"},
    {"toggle", "-p <pin>", "flip an output pin and report the level it reached"},
};

static const verb_t irq_verbs[] = {
    {"cfg", "-p <pin> -e <off|rising|falling|both>", "arm or disarm a trigger"},
    {"bind", "-p <pin> -e <rising|falling|both> -a <low|high|toggle> --to <pin>", "drive a pin from a trigger"},
    {"unbind", "-p <pin>", "drop a binding, leaving the trigger armed"},
};

static const verb_t timer_verbs[] = {
    {"cfg", "-T <0-2> -f <1-1000000>", "set a timer's frequency and start it"},
    {"get", "-T <0-2>", "read a timer's achieved frequency"},
    {"release", "-T <0-2>", "stop a timer and free its four pins"},
};

static const verb_t pwm_verbs[] = {
    {"cfg", "-p <pin> [--polarity <active-high|active-low>]", "claim a pin, silent at 0%"},
    {"set", "-p <pin> -u <percent>", "set a claimed pin's duty cycle"},
    {"get", "-p <pin>", "read back a pin's duty cycle"},
    {"release", "-p <pin>", "free one pin, leaving its timer running"},
};

static const verb_t mcu_verbs[] = {
    {"probe", "", "confirm mcu-co is on the other end"},
    {"reset", "", "reboot the co-processor"},
};

static const subsystem_t subsystems[] = {
    {"gpio", "pin direction, level and toggling", gpio_verbs, ARRAY_COUNT(gpio_verbs)},
    {"irq", "EXTI triggers and pin-to-pin bindings", irq_verbs, ARRAY_COUNT(irq_verbs)},
    {"timer", "PWM frequency groups", timer_verbs, ARRAY_COUNT(timer_verbs)},
    {"pwm", "per-pin duty cycle", pwm_verbs, ARRAY_COUNT(pwm_verbs)},
    {"mcu", "the co-processor itself", mcu_verbs, ARRAY_COUNT(mcu_verbs)},
};

static const subsystem_t *find_subsystem(const char *name)
{
    for (size_t index = 0; index < ARRAY_COUNT(subsystems); index++)
    {
        if (strcmp(subsystems[index].name, name) == 0)
        {
            return &subsystems[index];
        }
    }

    return NULL;
}

static const verb_t *find_verb(const subsystem_t *subsystem, const char *name)
{
    for (size_t index = 0; index < subsystem->verb_count; index++)
    {
        if (strcmp(subsystem->verbs[index].name, name) == 0)
        {
            return &subsystem->verbs[index];
        }
    }

    return NULL;
}

static void print_subsystems(FILE *stream)
{
    fprintf(stream, "usage: mcu-co-cli <subsystem> <verb> [flags]\n\n");

    for (size_t index = 0; index < ARRAY_COUNT(subsystems); index++)
    {
        fprintf(stream, "  %-7s %s\n", subsystems[index].name, subsystems[index].summary);
    }
}

static void print_verbs(FILE *stream, const subsystem_t *subsystem)
{
    fprintf(stream, "usage: mcu-co-cli %s <verb> [flags]\n\n", subsystem->name);

    for (size_t index = 0; index < subsystem->verb_count; index++)
    {
        fprintf(stream, "  %-8s %s\n", subsystem->verbs[index].name, subsystem->verbs[index].summary);
    }
}

static void print_verb(const subsystem_t *subsystem, const verb_t *verb)
{
    /* probe and reset take no flags, so the separator would otherwise leave a
     * trailing space. */
    const char *separator = (verb->flags[0] != '\0') ? " " : "";

    printf("usage: mcu-co-cli %s %s%s%s\n\n", subsystem->name, verb->name, separator, verb->flags);
    printf("  %s\n", verb->summary);
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        print_subsystems(stdout);
        return EXIT_OK;
    }

    const subsystem_t *subsystem = find_subsystem(argv[1]);

    if (subsystem == NULL)
    {
        fprintf(stderr, "mcu-co-cli: unknown subsystem '%s'\n\n", argv[1]);
        print_subsystems(stderr);
        return EXIT_USAGE;
    }

    if (argc < 3)
    {
        print_verbs(stdout, subsystem);
        return EXIT_OK;
    }

    const verb_t *verb = find_verb(subsystem, argv[2]);

    if (verb == NULL)
    {
        fprintf(stderr, "mcu-co-cli: unknown verb '%s' for %s\n\n", argv[2], subsystem->name);
        print_verbs(stderr, subsystem);
        return EXIT_USAGE;
    }

    print_verb(subsystem, verb);

    return EXIT_OK;
}
