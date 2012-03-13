/* See the file "COPYING" for the full license governing this code. */

#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "log.h"
#include "dcp.h"

#include "argparse.h"
#include "filestat.h"

#ifndef ARG_MAX
#define ARG_MAX _POSIX_ARG_MAX
#endif

/** Where we should store options specified by the user. */
DCOPY_options_t DCOPY_user_opts;

/**
 * Convert the destination to an absolute path and check sanity.
 */
void DCOPY_parse_dest_path(char* path)
{
    char dest_base[PATH_MAX];
    char file_name_buf[PATH_MAX];
    char norm_path[PATH_MAX];
    char* file_name;

    DCOPY_user_opts.dest_path = realpath(path, NULL);

    /*
     * If realpath doesn't work, we might be working with a file.
     */
    if(!DCOPY_user_opts.dest_path) {
        /* Since this might be a file, lets get the absolute base path. */
        strncpy(dest_base, path, PATH_MAX);
        DCOPY_user_opts.dest_path = dirname(dest_base);
        DCOPY_user_opts.dest_path = realpath(DCOPY_user_opts.dest_path, NULL);

        /* Save the length of the base for later use. */
        DCOPY_user_opts.dest_base_index = strlen(DCOPY_user_opts.dest_path);

        /* If realpath didn't work this time, we're really in trouble. */
        if(!DCOPY_user_opts.dest_path) {
            LOG(DCOPY_LOG_ERR, "Could not determine the path for `%s'. %s", \
                path, strerror(errno));
            exit(EXIT_FAILURE);
        }

        /* Now, lets get the base name. */
        strncpy(file_name_buf, path, PATH_MAX);
        file_name = basename(file_name_buf);

        /* Finally, lets put everything together. */
        sprintf(norm_path, "%s/%s", DCOPY_user_opts.dest_path, file_name);
        strncpy(DCOPY_user_opts.dest_path, norm_path, PATH_MAX);
    } else {
        /* Since this is a directory, the base index is the length. */
        DCOPY_user_opts.dest_base_index = strlen(DCOPY_user_opts.dest_path);
    }
}

/**
 * Grab the source paths.
 */
void DCOPY_parse_src_paths(char** argv, int last_arg_index, int optind)
{
    int index = 0;

    /*
     * Since we can't overwrite a file with a directory, lets see if the
     * destination is a file. When we go through all of the source arguments,
     * we can then check if we're trying to overwrite a file with a directory.
     */
    int destination_is_file = DCOPY_is_regular_file(DCOPY_user_opts.dest_path);

    /*
     * Loop over each source path and check sanity.
     */
    DCOPY_user_opts.src_path = (char**) malloc((ARG_MAX + 1) * sizeof(void*));
    memset(DCOPY_user_opts.src_path, 0, (ARG_MAX + 1) * sizeof(char));

    for(index = optind; index < last_arg_index; index++) {
        DCOPY_user_opts.src_path[index - optind] = realpath(argv[index], NULL);

        if(!DCOPY_user_opts.dest_path) {
            LOG(DCOPY_LOG_ERR, "Could not determine the path for `%s'. %s", \
                argv[index], strerror(errno));

            exit(EXIT_FAILURE);
        }

        if(destination_is_file && DCOPY_is_directory(DCOPY_user_opts.src_path[index - optind])) {
            LOG(DCOPY_LOG_ERR, "Cannot overwrite non-directory `%s' with directory `%s'",
                DCOPY_user_opts.dest_path, DCOPY_user_opts.src_path[index - optind]);

            exit(EXIT_FAILURE);
        }
    }
}

/**
 * Parse the source and destination paths that the user has provided.
 */
void DCOPY_parse_path_args(char** argv, int optind, int argc)
{
    size_t num_args = argc - optind;
    int last_arg_index = num_args + optind - 1;

    char** dbg_p = NULL;

    if(argv == NULL || num_args < 2) {
        DCOPY_print_usage(argv);
        LOG(DCOPY_LOG_ERR, "You must specify a source and destination path.");

        exit(EXIT_FAILURE);
    }

    /* Grab the destination path. */
    DCOPY_parse_dest_path(argv[last_arg_index]);

    /* Grab the source paths. */
    DCOPY_parse_src_paths(argv, last_arg_index, optind);

    /*
     * Now, lets print everything out for debugging purposes.
     */
    dbg_p = DCOPY_user_opts.src_path;

    while(*dbg_p != NULL) {
        LOG(DCOPY_LOG_DBG, "Found a source path with name: `%s'", *dbg_p);
        dbg_p++;
    }

    LOG(DCOPY_LOG_DBG, "Found a destination path with name: `%s'", DCOPY_user_opts.dest_path);
}

/* EOF */
