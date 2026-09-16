#ifndef STATUS_H
#define STATUS_H

/* Codes 1-10 are the firmware's status_t, sent verbatim as a NACK's reason
 * byte: fixed on the wire, append-only. Host-side codes start at 0x80 so they
 * can never collide with one. */
typedef enum
{
    STATUS_OK = 0,

    STATUS_ERR               = 1,
    STATUS_ERR_INVALID_ARG   = 2,
    STATUS_ERR_INVALID_PIN   = 3,
    STATUS_ERR_INVALID_STATE = 4,
    STATUS_ERR_NOT_INIT      = 5,
    STATUS_ERR_BUSY          = 6,
    STATUS_ERR_TIMEOUT       = 7,
    STATUS_ERR_UNSUPPORTED   = 8,
    STATUS_ERR_EMPTY         = 9,
    STATUS_ERR_FULL          = 10,

    /* Distinct from STATUS_ERR_INVALID_ARG so a local range check is
     * distinguishable from the MCU's own refusal. */
    STATUS_ERR_ARG = 0x80,

    STATUS_ERR_IO = 0x81,

    /* The MCU answers a bad frame with silence, so this cannot be told apart
     * from an absent MCU. */
    STATUS_ERR_NO_RESPONSE = 0x82,

    STATUS_ERR_BAD_FRAME = 0x83,
    STATUS_ERR_NOT_OPEN  = 0x84,
} mcu_status_t;

#endif /* STATUS_H */
