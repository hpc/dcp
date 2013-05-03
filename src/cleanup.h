/* See the file "COPYING" for the full license governing this code. */

#ifndef __DCP_CLEANUP_H
#define __DCP_CLEANUP_H

#include "common.h"

bool DCOPY_set_preserve_permissions(DCOPY_operation_t* op,
                                    CIRCLE_handle* handle,
                                    bool preserve_setxid,
                                    const struct stat64* statbuf,
                                    const char* dest_path);

bool DCOPY_set_preserve_timestamps(DCOPY_operation_t* op,
                                   CIRCLE_handle* handle,
                                   const struct stat64* statbuf,
                                   const char* dest_path);

bool DCOPY_set_preserve_ownership(DCOPY_operation_t* op,
                                  CIRCLE_handle* handle,
                                  const struct stat64* statbuf,
                                  const char* dest_path);

void DCOPY_truncate_file(DCOPY_operation_t* op,
                         CIRCLE_handle* handle);

void DCOPY_do_cleanup(DCOPY_operation_t* op,
                      CIRCLE_handle* handle);

#endif /* __DCP_CLEANUP_H */
