#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "mcuco_args.h"
#include "mcuco_command.h"
#include "mcuco.h"

typedef struct
{
    char *device_path;
    uint16_t timeout;
    bool help;
    bool version;
} config_t;

static void print_help(void)
{
    fprintf(stdout, "usage: mcu-co-cli <subsystem> <verb> <values...> [--options anywhere]\n");
    fprintf(stdout, "\n");
    fprintf(stdout, "  mcu    probe                                     check the link\n");
    fprintf(stdout, "         reset                                     reboot the MCU\n");
    fprintf(stdout, "\n");
    fprintf(stdout, "  gpio   cfg      <input|output> <pin>             set a pin's direction\n");
    fprintf(stdout, "         set      <low|high> <pin>                 drive an output pin\n");
    fprintf(stdout, "         get      <pin>                            -> low | high\n");
    fprintf(stdout, "         toggle   <pin>                            -> the level after the flip\n");
    fprintf(stdout, "\n");
    fprintf(stdout, "  irq    cfg      <off|rising|falling|both> <pin>  arm or disarm a trigger\n");
    fprintf(stdout, "         bind     <edge> <pin> <action> <pin>      drive one pin from another\n");
    fprintf(stdout, "         unbind   <pin>                            drop the action, stay armed\n");
    fprintf(stdout, "\n");
    fprintf(stdout, "  timer  cfg      <1-1000000> <0-2>                Hz, then which timer\n");
    fprintf(stdout, "         get      <0-2>                            -> achieved Hz\n");
    fprintf(stdout, "         release  <0-2>                            stop it, freezing its pins\n");
    fprintf(stdout, "\n");
    fprintf(stdout, "  pwm    cfg      <active-high|active-low> <pin>   claim a pin, silent at 0%%\n");
    fprintf(stdout, "         set      <0-100> <pin>                    percent, then the pin\n");
    fprintf(stdout, "         get      <pin>                            -> percent, one decimal\n");
    fprintf(stdout, "         release  <pin>                            free one pin\n");
    fprintf(stdout, "\n");
    fprintf(stdout, "  edge    off | rising | falling | both            off is invalid for bind\n");
    fprintf(stdout, "  action  low | high | toggle\n");
    fprintf(stdout, "  pin     two words, port then number: A 5 - ports A-G in capitals, 0-15\n");
    fprintf(stdout, "\n");
    fprintf(stdout, "pwm pins, by timer (from firmware peripherals/timer.c):\n");
    fprintf(stdout, "  timer 0   TIM2    A5     A1     B10    B11\n");
    fprintf(stdout, "  timer 1   TIM3    C6     C7     C8     C9\n");
    fprintf(stdout, "  timer 2   TIM4    B6     B7     B8     B9\n");
    fprintf(stdout, "\n");
    fprintf(stdout, "  Reference only. The MCU resolves which channel a pin is and refuses a pin\n");
    fprintf(stdout, "  with none; no command takes a timer for a pwm operation.\n");
    fprintf(stdout, "\n");
    fprintf(stdout, "options, valid on every command and in any position:\n");
    fprintf(stdout, "  -d, --device <path>    serial port    [$MCUCO_DEVICE, then /dev/ttyACM0]\n");
    fprintf(stdout, "  -t, --timeout <ms>     deadline       [1000]\n");
    fprintf(stdout, "  -h, --help\n");
    fprintf(stdout, "  -V, --version\n");
}

static int parse_timeout(const char *word, uint16_t *timeout_ms)
{
    errno               = 0;
    char *end           = NULL;
    unsigned long value = strtoul(word, &end, 10);

    if (errno != 0 || end == word || *end != '\0' || value < 1UL || value > UINT16_MAX)
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
                fprintf(stderr, "mcu-co-cli: bad timeout '%s' (1-%u ms)\n", optarg, (unsigned)UINT16_MAX);
                return -1;
            }
            break;

        case 'h':
            config->help = true;
            break;

        case 'V':
            config->version = true;
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
        print_help();
        return 0;
    }

    if (config.version == true)
    {
        // print_version();
        return 0;
    }

    mcuco_args_t mcuco_args = {0};
    if (args_parse_mcuco(argc - word_index, &argv[word_index], &mcuco_args) != 0)
    {
        fprintf(stderr, "mcu-co-cli: bad command\n");
        print_help();
        return 1;
    }

    mcuco_t *mcu = mcuco_open(config.device_path, config.timeout);
    if (mcu == NULL)
    {
        fprintf(stderr, "mcu-co-cli: could not open mcuco device\n");
        return 1;
    }

    mcu_status_t status = mcuco_run_command(mcu, &mcuco_args);

    mcuco_close(mcu);

    if (status != STATUS_OK)
    {
        fprintf(stderr, "%s\n", mcuco_strerror(status));
        return 1;
    }

    return 0;
}
