#ifndef MCUCO_COMMAND_H
#define MCUCO_COMMAND_H

#include <stdio.h>

#include "mcuco.h"
#include "mcuco_args.h"

/* The row for a subsystem and verb, or NULL when the pair is not a command. */
const command_t *mcuco_find_command(const char *subsystem, const char *verb);

/* Makes the library call the parsed command names, and prints any value it
 * returns. `mcu` must already be open. Returns the library's status unchanged,
 * or STATUS_ERR_ARG with nothing sent when no command was matched. */
mcu_status_t mcuco_run_command(mcuco_t *mcu, const mcuco_args_t *mcuco_args);

/* One line per command, grouped by subsystem, for --help. */
void mcuco_print_commands(FILE *stream);

#endif /* MCUCO_COMMAND_H */
