#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "commands.h"
#include "mcuco.h"

#define TIMEOUT_MIN_MS 20UL
#define TIMEOUT_MAX_MS 10000UL

typedef struct
{
    char *device_path;
    uint16_t timeout;
    bool help;
} config_t;

static void print_help(FILE *stream)
{
    fprintf(stream, "usage: mcu-co-cli <subsystem> <verb> <values...> [--options anywhere]\n");
    fprintf(stream, "\n");
    mcuco_print_commands(stream);
    fprintf(stream, "\n");
    fprintf(stream, "  a pin is two words, port then number: A 5 - ports A-G in capitals, 0-15\n");
    fprintf(stream, "  edge is off, rising, falling or both - bind takes all but off\n");
    fprintf(stream, "  action is low, high or toggle\n");
    fprintf(stream, "  polarity is active-high or active-low\n");
    fprintf(stream, "\n");
    fprintf(stream, "pwm pins, by timer (from firmware peripherals/timer.c):\n");
    fprintf(stream, "  timer 0   TIM2    A5     A1     B10    B11\n");
    fprintf(stream, "  timer 1   TIM3    C6     C7     C8     C9\n");
    fprintf(stream, "  timer 2   TIM4    B6     B7     B8     B9\n");
    fprintf(stream, "\n");
    fprintf(stream, "  Reference only. The MCU resolves which channel a pin is and refuses a pin\n");
    fprintf(stream, "  with none; no command takes a timer for a pwm operation.\n");
    fprintf(stream, "\n");
    fprintf(stream, "options, valid on every command and in any position:\n");
    fprintf(stream, "  -d, --device <path>    serial port    [default: /dev/ttyACM0]\n");
    fprintf(stream, "  -t, --timeout <ms>     deadline       [1000, %lu-%lu]\n", TIMEOUT_MIN_MS, TIMEOUT_MAX_MS);
    fprintf(stream, "  -h, --help\n");
}

static int parse_timeout(const char *word, uint16_t *timeout_ms)
{
    errno               = 0;
    char *end           = NULL;
    unsigned long value = strtoul(word, &end, 10);

    if (errno != 0 || end == word || *end != '\0' || value < TIMEOUT_MIN_MS || value > TIMEOUT_MAX_MS)
    {
        return -1;
    }

    *timeout_ms = (uint16_t)value;

    return 0;
}

static int parse_config(int argc, char **argv, config_t *config)
{
    static const struct option longopts[] = {
        {"device", required_argument, NULL, 'd'},
        {"timeout", required_argument, NULL, 't'},
        {"help", no_argument, NULL, 'h'},
        {"version", no_argument, NULL, 'V'},
        {NULL, 0, NULL, 0},
    };

    static const char *const shortopts = "d:t:hV";

    (void)memset(config, 0, sizeof(config_t));
    config->timeout     = 1000U;
    config->device_path = "/dev/ttyACM0";

    int opti = 0;
    int optc = 0;

    for (;;)
    {
        optc = getopt_long(argc, argv, shortopts, longopts, &opti);
        if (optc < 0)
        {
            break;
        }

        switch (optc)
        {
        case 'd':
            config->device_path = optarg;
            break;

        case 't':
            if (parse_timeout(optarg, &config->timeout) != 0)
            {
                fprintf(stderr, "mcu-co-cli: bad timeout '%s' (%lu-%lu ms)\n", optarg, TIMEOUT_MIN_MS, TIMEOUT_MAX_MS);
                return -1;
            }
            break;

        case 'h':
            config->help = true;
            break;

        default:
            return -1;
        }
    }

    return optind;
}

int main(int argc, char **argv)
{
    config_t config = {0};

    int word_index = parse_config(argc, argv, &config);

    if (word_index <= 0)
    {
        fprintf(stderr, "usage: mcu-co-cli <subsystem> <verb> <values...> [--options anywhere]\n");
        return 1;
    }

    if (config.help == true)
    {
        print_help(stdout);
        return 0;
    }

    mcuco_args_t mcuco_args = {0};
    int word_count  = argc - word_index;
    if (args_parse_mcuco(word_count, &argv[word_index], &mcuco_args) != 0)
    {
        fprintf(stderr, "mcu-co-cli: bad command\n");
        fprintf(stderr, "try 'mcu-co-cli --help'\n");
        return 1;
    }

    mcuco_t *mcu = mcuco_open(config.device_path, config.timeout);
    if (mcu == NULL)
    {
        fprintf(stderr, "mcu-co-cli: could not open mcuco device\n");
        return 1;
    }

    mcu_status_t status = mcuco_run_command(mcu, &mcuco_args);
    fprintf(stderr, "%s\n", mcuco_strerror(status));

    mcuco_close(mcu);

    return (status == STATUS_OK) ? 0 : 1;
}
