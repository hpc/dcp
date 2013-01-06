/*
 * This file contains the logic to
 * already exist. Since it would be redundant, we only pay attention to the
 * first chunk of each file and pass.

 * See the file "COPYING" for the full license governing this code.
 */

#include "cleanup.h"
#include "dcp.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

/** Options specified by the user. */
extern DCOPY_options_t DCOPY_user_opts;

void DCOPY_set_preserve_permissions(DCOPY_operation_t* op, CIRCLE_handle* handle)
{
    LOG(DCOPY_LOG_ERR, "Preserving ownership not implemented yet.");
    /* TODO: preserve permissions, requeue to cleanup if fail and unreliable. */
}

void DCOPY_set_preserve_ownership(DCOPY_operation_t* op, CIRCLE_handle* handle)
{
    LOG(DCOPY_LOG_ERR, "Preserving ownership not implemented yet.");
    /* TODO: preserve ownership, requeue to cleanup if fail and unreliable. */
}

void DCOPY_truncate_file(DCOPY_operation_t* op, CIRCLE_handle* handle)
{
    LOG(DCOPY_LOG_ERR, "Truncate not implemented yet. Files may be corrupt.");
    /* TODO: truncate file, requeue to cleanup if fail and unreliable. */
}

void DCOPY_do_cleanup(DCOPY_operation_t* op, CIRCLE_handle* handle)
{
    char* newop;

    /*
     * Only bother truncating and setting permissions on the first chunk of
     * the file.
     */
    if(op->chunk == 0) {

        if(DCOPY_user_opts.preserve) {
            DCOPY_set_preserve_permissions(op, handle);
            DCOPY_set_preserve_ownership(op, handle);
        }

        DCOPY_truncate_file(op, handle);
    }

    /*
     * If the user is feeling brave, this is where we let them skip the
     * comparison stage.
     */
    if(!DCOPY_user_opts.skip_compare) {
        newop = DCOPY_encode_operation(COMPARE, op->chunk, op->operand, \
                                       op->source_base_offset, \
                                       op->dest_base_appendix, op->file_size);

        handle->enqueue(newop);
        free(newop);
    }

    return;
}

/* EOF */
