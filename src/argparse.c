/* See the file "COPYING" for the full license governing this code. */

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "dcp.h"

#include "argparse.h"

#ifndef ARG_MAX
    #define ARG_MAX _POSIX_ARG_MAX
#endif

/** Where we should store options specified by the user. */
DCOPY_options_t DCOPY_user_opts;

/**
 * Parse the source and destination paths that the user has provided.
 */
void DCOPY_parse_path_args(char** argv, int optind, int argc)
{
    int index;
    size_t num_args = argc - optind;
    int last_arg_index = num_args + optind - 1;
    char** dbg_p;
    char* cwd_path;
    
    if(argv == NULL || num_args < 2) {
        DCOPY_print_usage(argv);
        LOG(DCOPY_LOG_ERR, "You must specify a source and destination path.");

        exit(EXIT_FAILURE);
    }

    /* The destination will always be the last item. */
    DCOPY_user_opts.dest_path = (char*) malloc(sizeof(char) * (PATH_MAX + 1));
    strncpy(DCOPY_user_opts.dest_path, argv[last_arg_index], PATH_MAX);

    /* Figure out if the dest path is absolute. */
    if(*DCOPY_user_opts.dest_path != '/') {
       cwd_path = (char*) malloc(sizeof(char) * PATH_MAX);

       if(!getcwd(cwd_path, PATH_MAX)) {
           LOG(DCOPY_LOG_ERR, "Could not determine the current working directory. %s", \
               strerror(errno));
       }

       sprintf(DCOPY_user_opts.dest_path, "%s/%s", cwd_path, argv[last_arg_index]);
       free(cwd_path);
    }

    /*
     * Since we can't overwrite a file with a directory, lets see if the
     * destination is a file.
     */
    int destination_is_file = DCOPY_is_regular_file(DCOPY_user_opts.dest_path);

    /* Now lets go back and get everything else for the source paths. */
    DCOPY_user_opts.src_path = (char**) malloc((ARG_MAX + 1) * sizeof(void *));
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

    /* Now we can print it out for debugging purposes. */
    dbg_p = DCOPY_user_opts.src_path;

    while(*dbg_p != NULL) {
        LOG(DCOPY_LOG_DBG, "Found a source path with name: `%s'", *dbg_p);
        dbg_p++;
    }

    LOG(DCOPY_LOG_DBG, "Found a destination path with name: `%s'", DCOPY_user_opts.dest_path);
}

/* EOF */
