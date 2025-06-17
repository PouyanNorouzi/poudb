#include "utils.h"

#include <stdio.h>  // For getchar and EOF

#define BUFFER_SIZE 512

/**
 * Handles when there is some input in stdin
 * returns 1 when the applitcaion should exit
 * otherwise returns 0
 */
int handle_stdin_input(void) {
    char c;
    int ret = 0;

    c = getchar();

    if(c == 'q' || c == 'Q') {
        ret = 1;
    }

    flush_stdin();

    return ret;
}

void flush_stdin(void) {
    int c;
    while((c = getchar()) != '\n' && c != EOF);
}
