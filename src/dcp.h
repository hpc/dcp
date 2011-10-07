#ifndef DCOPY_H
#define DCOPY_H

#include <unistd.h>
#include <stdint.h>

#ifndef DCOPY_FILECOPY_BUFFER_SIZE
    #define DCOPY_FILECOPY_BUFFER_SIZE (32 * 1024 * 1024) > (SIZE_MAX / 2) ? \
        (SIZE_MAX / 2) : (32 * 1024 * 1024)
#endif

void DCOPY_start(CIRCLE_handle *handle);
void DCOPY_copy (CIRCLE_handle *handle);

void print_usage(char *prog);

#endif /* DCOPY_H */
