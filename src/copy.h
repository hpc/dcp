/* See the file "COPYING" for the full license governing this code. */

#ifndef __DCP_COPY_H
#define __DCP_COPY_H

#include "common.h"

void DCOPY_do_copy(DCOPY_operation_t* op, \
                   CIRCLE_handle* handle);

int DCOPY_perform_copy(DCOPY_operation_t* op, \
                       int in_fd, \
                       int out_fd, \
                       off64_t offset);

void DCOPY_enqueue_cleanup_stage(DCOPY_operation_t* op, \
                                 CIRCLE_handle* handle);

#endif /* __DCP_COPY_H */
