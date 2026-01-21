#ifndef OPERATIONS_H
#define OPERATIONS_H

#include "parser.h"

/**
 * Execute a parsed command
 *
 * @param cmd Pointer to the command to execute
 * @return 0 on success, -1 on error
 */
int execute_command(Command* cmd);

#endif //OPERATIONS_H
