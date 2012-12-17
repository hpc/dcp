/* See the file "COPYING" for the full license governing this code. */

#ifndef __DCOPY_HANDLE_ARGS_H
#define __DCOPY_HANDLE_ARGS_H

#include <stdbool.h>
#include <libcircle.h>

#include "log.h"
#include "dcp.h"

#ifndef ARG_MAX
#define ARG_MAX _POSIX_ARG_MAX
#endif

bool DCOPY_dest_is_dir();
uint32_t DCOPY_source_file_count();
void DCOPY_parse_dest_path(char* path);
void DCOPY_parse_src_paths(char** argv, int last_arg_index, int optind);
void DCOPY_parse_path_args(char** argv, int optind, int argc);
void DCOPY_enqueue_work_objects(/* TODO: CIRCLE_handle* handle */);

#endif /* __DCOPY_HANDLE_ARGS_H */
