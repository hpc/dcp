/* See the file "COPYING" for the full license governing this code. */

#ifndef __DCP_COMPARE_H
#define __DCP_COMPARE_H

#include "common.h"

void DCOPY_do_compare(DCOPY_operation_t* op, \
                      CIRCLE_handle* handle);

int DCOPY_perform_compare(DCOPY_operation_t* op, \
                          FILE* in_ptr, \
                          FILE* out_ptr);

#endif /* __DCP_COMPARE_H */
