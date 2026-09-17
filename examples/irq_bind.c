#include <stdio.h>
#include <unistd.h>

#include "mcuco.h"
#include "status_name.h"

#define DEFAULT_DEVICE "/dev/ttyACM0"

/* USB-serial adapters drop bytes written immediately after open. */
#define SETTLE_US 100000

/* Nucleo-G474RE: B1 user button on PC13, LD2 on PA5. */
#define BUTTON_PORT PORT_C
#define BUTTON_PIN  13
#define LED_PORT    PORT_A
#define LED_PIN     5

static mcu_status_t step(const char *what, mcu_status_t status)
{
    printf("%-42s %s\n", what, status_name(status));

    return status;
}

/* Direction is never set implicitly, arming must precede binding, and the
 * bound edge must match the armed one exactly - so the order here is fixed. */
static mcu_status_t bind_button_to_led(mcuco_t *mcu)
{
    mcu_status_t status;

    status = step("gpio cfg input  C 13 (button)",
                  mcuco_gpio_cfg(mcu, DIR_INPUT, BUTTON_PORT, BUTTON_PIN));
    if (status != STATUS_OK)
        return status;

    status = step("gpio cfg output A 5  (LD2)",
                  mcuco_gpio_cfg(mcu, DIR_OUTPUT, LED_PORT, LED_PIN));
    if (status != STATUS_OK)
        return status;

    status = step("gpio irq cfg both C 13",
                  mcuco_gpio_irq_cfg(mcu, EDGE_BOTH, BUTTON_PORT, BUTTON_PIN));
    if (status != STATUS_OK)
        return status;

    /* TOGGLE is edge-agnostic, so one binding mirrors the button on the LED
     * without the ISR needing to know which edge fired. */
    return step("gpio irq bind both C 13 toggle A 5",
                mcuco_gpio_irq_bind(mcu, EDGE_BOTH, BUTTON_PORT, BUTTON_PIN,
                                    ACTION_TOGGLE, LED_PORT, LED_PIN));
}

int main(int argc, char **argv)
{
    const char *device_path = (argc > 1) ? argv[1] : DEFAULT_DEVICE;

    mcuco_t *mcu = mcuco_open(device_path, TIMEOUT_DEFAULT_MS);
    if (mcu == NULL)
    {
        perror("mcuco_open");
        return 1;
    }

    usleep(SETTLE_US);

    mcu_status_t status = bind_button_to_led(mcu);

    if (status == STATUS_OK)
    {
        printf("\nBound. The MCU drives LD2 from the button's ISR - press B1.\n");
        printf("The host is out of the loop now: this program can exit, and the\n");
        printf("USB cable can come out, and it will keep working.\n");
    }

    mcuco_close(mcu);

    return (status == STATUS_OK) ? 0 : 1;
}
