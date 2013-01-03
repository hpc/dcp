/* See the file "COPYING" for the full license governing this code. */

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "prepare.h"

/** Options specified by the user. */
extern DCOPY_options_t DCOPY_user_opts;

/** The loglevel that this instance of dcopy will output. */
extern DCOPY_loglevel  DCOPY_debug_level;

void DCOPY_do_prepare(DCOPY_operation_t* op, CIRCLE_handle* handle)
{
    char* newop;

    newop = DCOPY_encode_operation(COPY, op->chunk, op->operand, \
            op->source_base_offset, op->dest_base_appendix, op->file_size);
    handle->enqueue(newop);

    free(newop);

    return;
}

/* EOF */
