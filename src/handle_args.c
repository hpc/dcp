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

#include "handle_args.h"
#include "stat_file.h"

#ifndef ARG_MAX
#define ARG_MAX _POSIX_ARG_MAX
#endif

/** Where we should store options specified by the user. */
DCOPY_options_t DCOPY_user_opts;

/**
 * Analyze all file path inputs and place on the work queue.
 *
 * We start off with all of the following potential options in mind and prune
 * them until we figure out what situation we have.
 *
 * Source must overwrite destination.
 *   - Single file to single file
 *
 * Must return an error. Impossible condition.
 *   - Single directory to single file
 *   - Many file to single file
 *   - Many directory to single file
 *   - Many directory and many file to single file
 *
 * All Sources must be placed inside destination.
 *   - Single file to single directory
 *   - Single directory to single directory
 *   - Many file to single directory
 *   - Many directory to single directory
 *   - Many file and many directory to single directory
 *
 * @param handle the libcircle primary queue handle.
 */
void DCOPY_enqueue_work_objects(CIRCLE_handle* handle)
{
    bool dest_is_dir = DCOPY_dest_is_dir();
    bool dest_is_file  = !dest_is_dir;

    uint32_t number_of_source_files = DCOPY_source_file_count();

    LOG(DCOPY_LOG_DBG, "Found `%d' source files.", number_of_source_files);

    if(dest_is_file) {
        LOG(DCOPY_LOG_DBG, "Infered that the destination is a file.");

        /*
         * If the destination is a file, there must be only one source object, and it
         * must be a file.
         */
        if(number_of_source_files == 1 && DCOPY_is_regular_file(DCOPY_user_opts.src_path[0])) {
            /* Use the parent directory of the destination file as the base to write into. */
            DCOPY_user_opts.dest_base_index = strlen(dirname(DCOPY_user_opts.dest_path));

            LOG(DCOPY_LOG_DBG, "Enqueueing only source path `%s'.", DCOPY_user_opts.src_path[0]);
            char* op = DCOPY_encode_operation(STAT, 0, DCOPY_user_opts.src_path[0], 0);
            handle->enqueue(op);
        }
        else {
            /*
             * Determine if we're trying to copy one or more directories into
             * a file.
             */
            char** bad_src_path = DCOPY_user_opts.src_path;

            while(*bad_src_path != NULL) {
                if(DCOPY_is_directory(*(bad_src_path))) {
                    LOG(DCOPY_LOG_ERR, "Copying a directory into a file is not supported.");
                    LOG(DCOPY_LOG_ERR, "If the destination should be a directory, recursion must be specified.");

                    exit(EXIT_FAILURE);
                }

                bad_src_path++;
            }

            /*
             * The only remaining possible condition is that the user wants to
             * copy multiple files into a single file (hopefully).
             */
            LOG(DCOPY_LOG_ERR, "Copying several files into a single file is not supported.");
            exit(EXIT_FAILURE);
        }

    }
    else if(dest_is_dir) {
        LOG(DCOPY_LOG_DBG, "Infered that the destination is a directory.");

        /*
         * Since the destination is a directory, we use that as a base so we can
         * copy all of the source objects into it.
         */
        DCOPY_user_opts.dest_base_index = strlen(DCOPY_user_opts.dest_path);

        char** src_path = DCOPY_user_opts.src_path;

        while(*src_path != NULL) {
            LOG(DCOPY_LOG_DBG, "Enqueueing source path `%s'.", *(src_path));

            char* op = DCOPY_encode_operation(STAT, 0, *(src_path), 0);
            handle->enqueue(op);

            src_path++;
        }

    }
    else {
        /*
         * This is the catch-all for all of the object types we haven't
         * implemented yet.
         */
        LOG(DCOPY_LOG_ERR, "We've encountered an unsupported filetype.");
        exit(EXIT_FAILURE);
    }

    /* TODO: print mode we're using to DBG. */
}

/**
 * Determine if the destination path is a file or directory.
 *
 * It does this by first checking to see if an object is actually at the
 * destination path. If an object isn't already at the destination path, we
 * examine the source path(s) to determine the type of what the destination
 * path will be.
 *
 * @return true if the destination should be a directory, false otherwise.
 */
bool DCOPY_dest_is_dir()
{
    bool dest_path_is_dir = false;

    /*
     * First we need to determine if the last argument is a file or a directory.
     * We first attempt to see if the last argument already exists on disk. If it
     * doesn't, we then look at the sources to see if we can determine what the
     * last argument should be.
     */
    if(DCOPY_is_directory(DCOPY_user_opts.dest_path)) {
        dest_path_is_dir = true;

    }
    else if(DCOPY_is_regular_file(DCOPY_user_opts.dest_path)) {
        dest_path_is_dir = false;

    }
    else {
        /*
         * If recursion is turned on, we can have a file or a directory as the
         * destination.
         */
        if(DCOPY_user_opts.recursive || DCOPY_user_opts.recursive_unspecified) {
            /*
             * We can determine what the destination should be by looking at the
             * source arguments. If the source arguments contain a single file,
             * then the destination must be a single file. We prune out the
             * impossible combinations later on.
             */
            dest_path_is_dir = true;

            char** src_path = DCOPY_user_opts.src_path;

            while(*src_path != NULL) {
                if(DCOPY_is_regular_file(*(src_path))) {
                    dest_path_is_dir = false;
                }

                src_path++;
            }

        }
        else {
            /*
             * Since recursion is turned off, there's only potential to create a
             * file at the destination.
             */
            dest_path_is_dir = false;
        }
    }

    return dest_path_is_dir;
}

/**
 * Determine the count of source paths specified by the user.
 *
 * @return the number of source paths specified by the user.
 */
uint32_t DCOPY_source_file_count()
{
    uint32_t source_file_count = 0;
    char** src_path = DCOPY_user_opts.src_path;

    while(*src_path != NULL) {
        source_file_count++;
        src_path++;
    }

    return source_file_count;
}

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
    }
}

/**
 * Grab the source paths.
 */
void DCOPY_parse_src_paths(char** argv, int last_arg_index, int optind_local)
{
    int opt_index = 0;

    /*
     * Loop over each source path and check sanity.
     */
    DCOPY_user_opts.src_path = (char**) malloc((ARG_MAX + 1) * sizeof(void*));
    memset(DCOPY_user_opts.src_path, 0, (ARG_MAX + 1) * sizeof(char));

    for(opt_index = optind_local; opt_index < last_arg_index; opt_index++) {
        DCOPY_user_opts.src_path[opt_index - optind_local] = realpath(argv[opt_index], NULL);

        if(!DCOPY_user_opts.dest_path) {
            LOG(DCOPY_LOG_ERR, "Could not determine the path for `%s'. %s", \
                argv[opt_index], strerror(errno));

            exit(EXIT_FAILURE);
        }
    }
}

/**
 * Parse the source and destination paths that the user has provided.
 */
void DCOPY_parse_path_args(char** argv, int optind_local, int argc)
{
    size_t num_args = argc - optind_local;
    int last_arg_index = num_args + optind_local - 1;

    if(argv == NULL || num_args < 2) {
        DCOPY_print_usage(argv);
        LOG(DCOPY_LOG_ERR, "You must specify a source and destination path.");

        exit(EXIT_FAILURE);
    }

    /* Grab the destination path. */
    DCOPY_parse_dest_path(argv[last_arg_index]);

    /* Grab the source paths. */
    DCOPY_parse_src_paths(argv, last_arg_index, optind_local);

    /*
     * Now, lets print everything out for debugging purposes.
     */
    char** dbg_p = DCOPY_user_opts.src_path;

    while(*dbg_p != NULL) {
        LOG(DCOPY_LOG_DBG, "Found a source path with name: `%s'", *(dbg_p));
        dbg_p++;
    }

    LOG(DCOPY_LOG_DBG, "Found a destination path with name: `%s'", DCOPY_user_opts.dest_path);
}

/* EOF */
