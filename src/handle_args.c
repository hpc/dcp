/*
 * See the file "COPYING" for the full license governing this code.
 *
 * For an overview of how argument handling was originally designed in dcp,
 * please see the blog post at:
 *
 * <http://www.bringhurst.org/2012/12/16/file-copy-tool-argument-handling.html>
 */

#include "handle_args.h"
#include "treewalk.h"
#include "dcp.h"
#include "bayer.h"

#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/** Where we should store options specified by the user. */
DCOPY_options_t DCOPY_user_opts;

typedef struct param_file {
    char* orig;                /* original path as specified by user */
    char* path;                /* reduced path, but still includes symlinks */
    int   path_stat_valid;     /* flag to indicate whether path_stat is valid */
    struct stat64 path_stat;   /* stat of path */
    char* target;              /* fully resolved path, no more symlinks */
    int   target_stat_valid;   /* flag to indicate whether target_stat is valid */
    struct stat64 target_stat; /* stat of target path */
} param_file_t;

static param_file_t  dest_param;
static param_file_t* src_params;
static int num_src_params;

/* initialize fields in param */
static void DCOPY_param_init(param_file_t* param)
{
    /* initialize all fields */
    if(param != NULL) {
        param->orig = NULL;
        param->path = NULL;
        param->path_stat_valid = 0;
        param->target = NULL;
        param->target_stat_valid = 0;
    }

    return;
}

/* set fields in param according to path */
static void DCOPY_param_set(const char* path, param_file_t* param)
{
    /* initialize all fields */
    DCOPY_param_init(param);

    if(path != NULL) {
        /* make a copy of original path */
        param->orig = bayer_strdup(path, "original path", __FILE__, __LINE__);

        /* get absolute path and remove ".", "..", consecutive "/",
         * and trailing "/" characters */
        bayer_path* p = bayer_path_from_str(path);
        if(! bayer_path_is_absolute(p)) {
            char cwd[PATH_MAX];
            bayer_getcwd(cwd, PATH_MAX);
            bayer_path_prepend_str(p, cwd);
        }
        bayer_path_reduce(p);
        param->path = bayer_path_strdup(p);
        bayer_path_delete(&p);

        /* get stat info for simplified path */
        if(lstat64(param->path, &param->path_stat) == 0) {
            param->path_stat_valid = 1;
        }

        /* TODO: we use realpath below, which is nice since it takes out
         * ".", "..", symlinks, and adds the absolute path, however, it
         * fails if the file/directory does not already exist, which is
         * often the case for dest path. */

        /* resolve any symlinks */
        char target[PATH_MAX];
        if(realpath(path, target) != NULL) {
            /* make a copy of resolved name */
            param->target = bayer_strdup(target, "target path", __FILE__, __LINE__);

            /* get stat info for resolved path */
            if(lstat64(param->target, &param->target_stat) == 0) {
                param->target_stat_valid = 1;
            }
        }
    }

    return;
}

/* free memory associated with param */
static void DCOPY_param_free(param_file_t* param)
{
    if(param != NULL) {
        /* free all mememory */
        bayer_free(&param->orig);
        bayer_free(&param->path);
        bayer_free(&param->target);

        /* initialize all fields */
        DCOPY_param_init(param);
    }
    return;
}

/**
 * Determine if the specified path is a directory.
 */
static bool DCOPY_is_directory(char* path)
{
    struct stat64 statbuf;

    if(lstat64(path, &statbuf) < 0) {
        /* LOG(DCOPY_LOG_ERR, "Could not determine if `%s' is a directory. %s", path, strerror(errno)); */
        return false;
    }

    return (S_ISDIR(statbuf.st_mode));
}

/**
 * Determine if the specified path is a regular file.
 */
static bool DCOPY_is_regular_file(char* path)
{
    struct stat64 statbuf;

    if(lstat64(path, &statbuf) < 0) {
        /* LOG(DCOPY_LOG_ERR, "Could not determine if `%s' is a file. %s", path, strerror(errno)); */
        return false;
    }

    return (S_ISREG(statbuf.st_mode));
}

/**
 * Determine if the specified path is a regular file.
 */
static bool DCOPY_is_link(char* path)
{
    struct stat64 statbuf;

    if(lstat64(path, &statbuf) < 0) {
        /* LOG(DCOPY_LOG_ERR, "Could not determine if `%s' is a file. %s", path, strerror(errno)); */
        return false;
    }

    return (S_ISLNK(statbuf.st_mode));
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
static bool DCOPY_dest_is_dir(void)
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

            int i;

            for(i = 0; i < num_src_params; i++) {
                char* src_path = src_params[i].path;

                if(DCOPY_is_regular_file(src_path)) {
                    dest_path_is_dir = false;
                }
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
 * Check if the current user has access to all source paths, then determine
 * the count of all source paths specified by the user.
 *
 * @return the number of source paths specified by the user.
 */
static uint32_t DCOPY_source_file_count(void)
{
    uint32_t source_file_count = 0;

    int i;

    for(i = 0; i < num_src_params; i++) {
        char* src_path = src_params[i].path;

        if(access(src_path, R_OK) < 0) {
            LOG(DCOPY_LOG_ERR, "Could not access source file at `%s'. %s", \
                src_path, strerror(errno));
        }
        else {
            source_file_count++;
        }

    }

    return source_file_count;
}

/**
 * Analyze all file path inputs and place on the work queue.
 *
 * We start off with all of the following potential options in mind and prune
 * them until we figure out what situation we have.
 *
 * Libcircle only calls this function from rank 0, so there's no need to check
 * the current rank here.
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

    char* opts_dest_path_dirname;
    char* src_path_dirname;

    uint32_t number_of_source_files = DCOPY_source_file_count();

    if(number_of_source_files < 1) {
        LOG(DCOPY_LOG_ERR, "At least one valid source file must be specified.");
        DCOPY_abort(EXIT_FAILURE);
    }

    if(dest_is_file) {
        LOG(DCOPY_LOG_DBG, "Infered that the destination is a file.");

        /*
         * If the destination is a file, there must be only one source object, and it
         * must be a file.
         */
        if(number_of_source_files == 1 && DCOPY_is_regular_file(src_params[0].path)) {
            /* Make a copy of the dest path so we can run dirname on it. */
            size_t dest_size = sizeof(char) * PATH_MAX;
            opts_dest_path_dirname = (char*) malloc(dest_size);

            if(opts_dest_path_dirname == NULL) {
                LOG(DCOPY_LOG_DBG, "Failed to allocate %llu bytes for dest path.", (long long unsigned) dest_size);
                DCOPY_abort(EXIT_FAILURE);
            }

            int dest_written = snprintf(opts_dest_path_dirname, dest_size, "%s", DCOPY_user_opts.dest_path);

            if(dest_written < 0 || (size_t)(dest_written) > dest_size - 1) {
                LOG(DCOPY_LOG_DBG, "Destination path too long.");
                DCOPY_abort(EXIT_FAILURE);
            }

            opts_dest_path_dirname = dirname(opts_dest_path_dirname);

            /* Make a copy of the src path so we can run dirname on it. */
            size_t src_size = sizeof(char) * PATH_MAX;
            src_path_dirname = (char*) malloc(sizeof(char) * PATH_MAX);

            if(src_path_dirname == NULL) {
                LOG(DCOPY_LOG_DBG, "Failed to allocate %llu bytes for dest path.", (long long unsigned) src_size);
                DCOPY_abort(EXIT_FAILURE);
            }

            int src_written = snprintf(src_path_dirname, src_size, "%s", src_params[0].path);

            if(src_written < 0 || (size_t)(src_written) > src_size - 1) {
                LOG(DCOPY_LOG_DBG, "Source path too long.");
                DCOPY_abort(EXIT_FAILURE);
            }

            src_path_dirname = dirname(src_path_dirname);

            /* LOG(DCOPY_LOG_DBG, "Enqueueing only a single source path `%s'.", DCOPY_user_opts.src_path[0]); */
            char* op = DCOPY_encode_operation(TREEWALK, 0, src_params[0].path, \
                                              (uint16_t)strlen(src_path_dirname), NULL, 0);

            handle->enqueue(op);
            free(op);

            free(opts_dest_path_dirname);
            free(src_path_dirname);
        }
        else {
            /*
             * Determine if we're trying to copy one or more directories into
             * a file.
             */
            int i;

            for(i = 0; i < num_src_params; i++) {
                char* src_path = src_params[i].path;

                if(DCOPY_is_directory(src_path)) {
                    LOG(DCOPY_LOG_ERR, "Copying a directory into a file is not supported.");
                    DCOPY_abort(EXIT_FAILURE);
                }
            }

            /*
             * The only remaining possible condition is that the user wants to
             * copy multiple files into a single file (hopefully).
             */
            LOG(DCOPY_LOG_ERR, "Copying several files into a single file is not supported.");
            DCOPY_abort(EXIT_FAILURE);
        }
    }
    else if(dest_is_dir) {
        LOG(DCOPY_LOG_DBG, "Infered that the destination is a directory.");
        bool dest_already_exists = DCOPY_is_directory(DCOPY_user_opts.dest_path);

        int i;

        for(i = 0; i < num_src_params; i++) {
            char* src_path = src_params[i].path;
            LOG(DCOPY_LOG_DBG, "Enqueueing source path `%s'.", src_path);

            char* src_path_basename = NULL;
            size_t src_len = strlen(src_path) + 1;
            char* src_path_basename_tmp = (char*) malloc(src_len);

            if(src_path_basename_tmp == NULL) {
                LOG(DCOPY_LOG_ERR, "Failed to allocate tmp for src_path_basename.");
                DCOPY_abort(EXIT_FAILURE);
            }

            /*
             * If the destination directory already exists, we want to place
             * new files inside it. To do this, we send a path fragment along
             * with the source path message and append it to the options dest
             * path whenever the options dest path is used.
             */
            if(dest_already_exists && !DCOPY_user_opts.conditional) {
                /* Make a copy of the src path so we can run basename on it. */
                strncpy(src_path_basename_tmp, src_path, src_len);
                src_path_basename = basename(src_path_basename_tmp);
            }

            char* op = DCOPY_encode_operation(TREEWALK, 0, src_path, \
                                              (uint16_t)(src_len - 1), \
                                              src_path_basename, 0);
            handle->enqueue(op);
            free(op);

            free(src_path_basename_tmp);
        }
    }
    else {
        /*
         * This is the catch-all for all of the object types we haven't
         * implemented yet.
         */
        LOG(DCOPY_LOG_ERR, "We've encountered an unsupported filetype.");
        DCOPY_abort(EXIT_FAILURE);
    }

    /* TODO: print mode we're using to DBG. */
}

/**
 * Rank 0 passes in pointer to string to be bcast -- all others pass in a
 * pointer to a string which will be newly allocated and filled in with copy.
 */
static bool DCOPY_bcast_str(char* send, char** recv)
{
    /* First, we broadcast the number of characters in the send string. */
    int len = 0;

    if(recv == NULL) {
        LOG(DCOPY_LOG_ERR, "Attempted to receive a broadcast into invalid memory. " \
                "Please report this as a bug!");
        return false;
    }

    if(CIRCLE_global_rank == 0) {
        if(send != NULL) {
            len = (int)(strlen(send) + 1);

            if(len > CIRCLE_MAX_STRING_LEN) {
                LOG(DCOPY_LOG_ERR, "Attempted to send a larger string (`%d') than what "
                        "libcircle supports. Please report this as a bug!", len);
                return false;
            }
        }
    }

    if(MPI_SUCCESS != MPI_Bcast(&len, 1, MPI_INT, 0, MPI_COMM_WORLD)) {
        LOG(DCOPY_LOG_DBG, "While preparing to copy, broadcasting the length of a string over MPI failed.");
        return false;
    }

    /* If the string is non-zero bytes, allocate space and bcast it. */
    if(len > 0) {
        /* allocate space to receive string */
        *recv = (char*) malloc((size_t)len);

        if(*recv == NULL) {
            LOG(DCOPY_LOG_ERR, "Failed to allocate string of %d bytes", len);
            return false;
        }

        /* Broadcast the string. */
        if(CIRCLE_global_rank == 0) {
            strncpy(*recv, send, len);
        }

        if(MPI_SUCCESS != MPI_Bcast(*recv, len, MPI_CHAR, 0, MPI_COMM_WORLD)) {
            LOG(DCOPY_LOG_DBG, "While preparing to copy, broadcasting the length of a string over MPI failed.");
            return false;
        }

    }
    else {
        /* Root passed in a NULL value, so set the output to NULL. */
        *recv = NULL;
    }

    return true;
}

/**
 * Convert the destination to an absolute path and check sanity.
 */
static void DCOPY_parse_dest_path(char* path)
{
    /* identify destination path */
    char dest_path[PATH_MAX];

    /* standardize destination path */
    if(CIRCLE_global_rank == 0) {
        DCOPY_param_set(path, &dest_param);
        strncpy(dest_path, dest_param.path, sizeof(dest_path));
    }

    /* Copy the destination path to user opts structure on each rank. */
    if(!DCOPY_bcast_str(dest_path, &DCOPY_user_opts.dest_path)) {
        LOG(DCOPY_LOG_ERR, "Could not send the proper destination path to other nodes (`%s'). " \
            "The MPI broadcast operation failed. %s", \
            path, strerror(errno));
        DCOPY_abort(EXIT_FAILURE);
    }

    return;
}

/**
 * Grab the source paths.
 */
static void DCOPY_parse_src_paths(char** argv, \
                                  int last_arg_index, \
                                  int optind_local)
{
    /* allocate memory to store pointers to source path info */
    src_params = NULL;
    num_src_params = last_arg_index - optind_local;

    /* only rank 0 resolves the path(s) */
    if(CIRCLE_global_rank == 0) {
        if(num_src_params > 0) {
            size_t src_params_bytes = (size_t)(num_src_params) * sizeof(param_file_t);
            src_params = (param_file_t*) bayer_malloc(src_params_bytes, "sources", __FILE__, __LINE__);
        }

        /* Loop over each source path and check sanity. */
        int opt_index;
        for(opt_index = optind_local; opt_index < last_arg_index; opt_index++) {
            char* path = argv[opt_index];

            /* get index into source param array */
            int idx = opt_index - optind_local;

            DCOPY_param_set(path, &src_params[idx]);

            /* TODO: verify that each source path is readable */
        }
    }

    return;
}

/**
 * Parse the source and destination paths that the user has provided.
 */
void DCOPY_parse_path_args(char** argv, \
                           int optind_local, \
                           int argc)
{
    int num_args = argc - optind_local;
    int last_arg_index = num_args + optind_local - 1;

    if(argv == NULL || num_args < 2) {
        if(CIRCLE_global_rank == 0) {
            DCOPY_print_usage(argv);
            LOG(DCOPY_LOG_ERR, "You must specify a source and destination path.");
        }

        DCOPY_exit(EXIT_FAILURE);
    }

    /* Grab the destination path. */
    DCOPY_parse_dest_path(argv[last_arg_index]);

    /* Grab the source paths. */
    DCOPY_parse_src_paths(argv, last_arg_index, optind_local);
}

/* frees resources allocated in call to parse_path_args() */
void DCOPY_free_path_args()
{
    /* only rank 0 allocated memory */
    if(CIRCLE_global_rank == 0) {
        /* free memory associated with destination path */
        DCOPY_param_free(&dest_param);

        /* free memory associated with source paths */
        int i;
        for(i = 0; i < num_src_params; i++) {
            DCOPY_param_free(&src_params[i]);
        }
        num_src_params = 0;
        bayer_free(&src_params);
    }
}

/* EOF */
