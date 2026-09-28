#ifndef MCUCO_COMMAND_H
#define MCUCO_COMMAND_H

#include "mcuco.h"
#include "mcuco_args.h"

/* Makes the library call a parsed command names, and prints any value it
 * returns. `mcu` must already be open. Returns the library's status
 * unchanged. */
mcu_status_t mcuco_run_command(mcuco_t *mcu, const mcuco_args_t *mcuco_args);

#endif /* MCUCO_COMMAND_H */
