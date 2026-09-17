#include <stdio.h>
#include <unistd.h>

#include "mcuco.h"
#include "status_name.h"

#define DEFAULT_DEVICE "/dev/ttyACM0"

/* USB-serial adapters drop bytes written immediately after open. */
#define SETTLE_US 100000

#define BLINKS 5

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

    printf("gpio cfg output A 5 ... ");
    fflush(stdout);

    mcu_status_t status = mcuco_gpio_cfg(mcu, DIR_OUTPUT, PORT_A, 5);
    printf("%s\n", status_name(status));

    /* PA5 is LD2 on the Nucleo, so this is visible. */
    // for (int i = 0; i < BLINKS && status == STATUS_OK; i++)
    while (status == STATUS_OK)
    {
        printf("gpio set high A 5 ... ");
        fflush(stdout);
        status = mcuco_gpio_set(mcu, LEVEL_HIGH, PORT_A, 5);
        printf("%s\n", status_name(status));
        sleep(1);

        if (status != STATUS_OK)
            break;

        printf("gpio set low  A 5 ... ");
        fflush(stdout);
        status = mcuco_gpio_set(mcu, LEVEL_LOW, PORT_A, 5);
        printf("%s\n", status_name(status));
        sleep(1);
    }

    mcuco_close(mcu);

    return (status == STATUS_OK) ? 0 : 1;
}
