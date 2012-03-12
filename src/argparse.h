/* See the file "COPYING" for the full license governing this code. */

#ifndef __DCOPY_ARGPARSE_H
#define __DCOPY_ARGPARSE_H

#include <stdbool.h>

#include "log.h"
#include "dcp.h"

#ifndef ARG_MAX
#define ARG_MAX _POSIX_ARG_MAX
#endif

void DCOPY_parse_dest_path(char* path);
void DCOPY_parse_src_paths(char** argv, int last_arg_index, int optind);
void DCOPY_parse_path_args(char** argv, int optind, int argc);

#endif /* __DCOPY_ARGPARSE_H */
