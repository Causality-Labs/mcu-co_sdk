#include "mcuco.h"

const char *mcuco_strerror(mcu_status_t status)
{
    switch (status)
    {
    case MCUCO_STATUS_OK:
        return "Success";

    /* Reported by the MCU as a NACK reason. */
    case MCUCO_STATUS_ERR:
        return "Unspecified failure on the MCU";
    case MCUCO_STATUS_ERR_INVALID_ARG:
        return "The MCU rejected a field as out of range";
    case MCUCO_STATUS_ERR_INVALID_PIN:
        return "Pin number out of range, or a reserved pin";
    case MCUCO_STATUS_ERR_INVALID_STATE:
        return "The pin is not configured for this operation";
    case MCUCO_STATUS_ERR_NOT_INIT:
        return "The peripheral was never brought up";
    case MCUCO_STATUS_ERR_BUSY:
        return "The resource is already in use";
    case MCUCO_STATUS_ERR_TIMEOUT:
        return "MCU hardware did not respond in time";
    case MCUCO_STATUS_ERR_UNSUPPORTED:
        return "Unknown opcode, or a request the hardware cannot satisfy";
    case MCUCO_STATUS_ERR_EMPTY:
        return "No data available on the MCU";
    case MCUCO_STATUS_ERR_FULL:
        return "No space available on the MCU";

    /* Raised here; these never travel on the wire. */
    case MCUCO_STATUS_ERR_ARG:
        return "An argument was rejected before anything was sent";
    case MCUCO_STATUS_ERR_IO:
        return "Reading or writing the serial port failed";
    case MCUCO_STATUS_ERR_NO_RESPONSE:
        return "No response before the timeout, or the MCU rejected the frame";
    case MCUCO_STATUS_ERR_BAD_FRAME:
        return "The response was malformed or failed its checksum";
    case MCUCO_STATUS_ERR_NOT_OPEN:
        return "The connection is not open";

    default:
        return "Unknown status code";
    }
}
