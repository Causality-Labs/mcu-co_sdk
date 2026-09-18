#include <stdio.h>
#include <unistd.h>

#include "mcuco.h"
#include "status_name.h"

#define DEFAULT_DEVICE "/dev/ttyACM0"

/* USB-serial adapters drop bytes written immediately after open. */
#define SETTLE_US 100000

/* The prescaler and reload are integers, so not every requested frequency is
 * reachable. One group per request: reconfiguring a live group is refused, and
 * there is no release command yet. */
static const struct
{
    uint8_t group;
    uint32_t freq_hz;
} REQUESTS[] = {
    {0, 1000},
    {1, 60000},
    {2, 7777},
};

static void report(uint8_t group, uint32_t requested_hz, mcuco_t *mcu)
{
    mcu_status_t status = mcuco_pwm_group_cfg(mcu, requested_hz, group);

    /* The group keeps whatever it was already set to, so the read below still
     * says something useful. */
    if (status != STATUS_OK && status != STATUS_ERR_BUSY)
    {
        printf("group %u: requested %7u Hz -> cfg %s\n", group, requested_hz, status_name(status));
        return;
    }

    uint32_t achieved_hz     = 0;
    mcu_status_t read_status = mcuco_pwm_group_get(mcu, group, &achieved_hz);

    if (read_status != STATUS_OK)
    {
        printf("group %u: requested %7u Hz -> cfg %s, get %s\n", group, requested_hz, status_name(status),
               status_name(read_status));
        return;
    }

    long drift = (long)achieved_hz - (long)requested_hz;

    printf("group %u: requested %7u Hz -> cfg %-5s achieved %7u Hz  (%+ld)\n", group, requested_hz, status_name(status),
           achieved_hz, drift);
}

int main(int argc, char **argv)
{
    const char *device_path = (argc > 1) ? argv[1] : DEFAULT_DEVICE;

    printf("opening %s\n", device_path);

    mcuco_t *mcu = mcuco_open(device_path, TIMEOUT_DEFAULT_MS);
    if (mcu == NULL)
    {
        perror("mcuco_open");
        return 1;
    }

    usleep(SETTLE_US);

    for (size_t i = 0; i < sizeof(REQUESTS) / sizeof(REQUESTS[0]); i++)
        report(REQUESTS[i].group, REQUESTS[i].freq_hz, mcu);

    mcuco_close(mcu);

    return 0;
}
