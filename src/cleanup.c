/*
 * This file contains the logic to 
 * already exist. Since it would be redundant, we only pay attention to the
 * first chunk of each file and pass.
 
 * See the file "COPYING" for the full license governing this code.
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "cleanup.h"

/** Options specified by the user. */
extern DCOPY_options_t DCOPY_user_opts;

void DCOPY_do_cleanup(DCOPY_operation_t* op, CIRCLE_handle* handle)
{
    char* newop;

    /* TODO: preserve attributes and truncate (only on chunk 0s). */

    /*
     * If the user is feeling brave, this is where we let them skip the
     * comparison stage.
     */
    if(!DCOPY_user_opts.skip_compare) {
        newop = DCOPY_encode_operation(COMPARE, op->chunk, op->operand, \
                op->source_base_offset, op->dest_base_appendix, op->file_size);

        handle->enqueue(newop);
        free(newop);
    }

    return;
}

/* EOF */
