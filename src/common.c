/* See the file "COPYING" for the full license governing this code. */

#include "common.h"
#include "handle_args.h"

#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/** The loglevel that this instance of dcopy will output. */
DCOPY_loglevel DCOPY_debug_level;

/** Where we should keep statistics related to this file copy. */
DCOPY_statistics_t DCOPY_statistics;

/** Where we should store options specified by the user. */
DCOPY_options_t DCOPY_user_opts;

/** Where debug output should go. */
FILE* DCOPY_debug_stream;

/** What rank the current process is. */
int CIRCLE_global_rank;

/** A table of function pointers used for core operation. */
void (*DCOPY_jump_table[5])(DCOPY_operation_t* op, CIRCLE_handle* handle);

void DCOPY_retry_failed_operation(DCOPY_operation_code_t target, \
                                  CIRCLE_handle* handle, \
                                  DCOPY_operation_t* op)
{
    char* new_op;

    if(DCOPY_user_opts.reliable_filesystem) {
        LOG(DCOPY_LOG_ERR, "Not retrying failed operation. " \
            "Reliable filesystem is specified.");
        DCOPY_abort(EXIT_FAILURE);
    }
    else {
        LOG(DCOPY_LOG_INFO, "Attempting to retry operation.");

        new_op = DCOPY_encode_operation(target, op->chunk, op->operand, \
                                        op->source_base_offset, \
                                        op->dest_base_appendix, op->file_size);

        handle->enqueue(new_op);
        free(new_op);
    }

    return;
}

/**
 * Encode an operation code for use on the distributed queue structure.
 */
char* DCOPY_encode_operation(DCOPY_operation_code_t code, \
                             int64_t chunk, \
                             char* operand, \
                             uint16_t source_base_offset, \
                             char* dest_base_appendix, \
                             int64_t file_size)
{
    char* op = (char*) malloc(sizeof(char) * CIRCLE_MAX_STRING_LEN);
    int op_size = 0;

    op_size += sprintf(op, "%" PRId64 ":%" PRId64 ":%" PRIu16 ":%d:%s", \
                       file_size, chunk, source_base_offset, code, operand);

    if(dest_base_appendix) {
        op_size += sprintf(op + op_size, ":%s", dest_base_appendix);
    }

    /*
     * FIXME: This requires architecture changes in libcircle -- a redesign of
     * internal queue data structures to allow void* types as queue elements
     * instead of null terminated strings. Ignoring this problem by commenting
     * out this check will likely cause silent data corruption.
     */
    if((op_size + 1) > CIRCLE_MAX_STRING_LEN) {
        LOG(DCOPY_LOG_DBG, \
            "Exceeded libcircle message size due to large file path. " \
            "This is a known bug in dcp that we intend to fix. Sorry!");
        DCOPY_abort(EXIT_FAILURE);
    }

    return op;
}

/**
 * Decode the operation code from a message on the distributed queue structure.
 */
DCOPY_operation_t* DCOPY_decode_operation(char* op)
{
    DCOPY_operation_t* ret = (DCOPY_operation_t*) malloc(sizeof(DCOPY_operation_t));

    if(sscanf(strtok(op, ":"), "%" SCNd64, &(ret->file_size)) != 1) {
        LOG(DCOPY_LOG_ERR, "Could not decode file size attribute.");
        DCOPY_abort(EXIT_FAILURE);
    }

    if(sscanf(strtok(NULL, ":"), "%" SCNd64, &(ret->chunk)) != 1) {
        LOG(DCOPY_LOG_ERR, "Could not decode chunk index attribute.");
        DCOPY_abort(EXIT_FAILURE);
    }

    if(sscanf(strtok(NULL, ":"), "%" SCNu16, &(ret->source_base_offset)) != 1) {
        LOG(DCOPY_LOG_ERR, "Could not decode source base offset attribute.");
        DCOPY_abort(EXIT_FAILURE);
    }

    if(sscanf(strtok(NULL, ":"), "%d", (int*) & (ret->code)) != 1) {
        LOG(DCOPY_LOG_ERR, "Could not decode stage code attribute.");
        DCOPY_abort(EXIT_FAILURE);
    }

    ret->operand            = strtok(NULL, ":");
    ret->dest_base_appendix = strtok(NULL, ":");

    /* build destination object name */
    char dest_path_recursive[PATH_MAX];
    char dest_path_file_to_file[PATH_MAX];
    if(ret->dest_base_appendix == NULL) {
        sprintf(dest_path_recursive, "%s/%s", \
                DCOPY_user_opts.dest_path, \
                ret->operand + ret->source_base_offset + 1);

        strncpy(dest_path_file_to_file, DCOPY_user_opts.dest_path, PATH_MAX);
    }
    else {
        sprintf(dest_path_recursive, "%s/%s/%s", \
                DCOPY_user_opts.dest_path, \
                ret->dest_base_appendix, \
                ret->operand + ret->source_base_offset + 1);

        sprintf(dest_path_file_to_file, "%s/%s", \
                DCOPY_user_opts.dest_path, \
                ret->dest_base_appendix);
    }
    ret->dest_full_path = strdup(dest_path_recursive);
    if(ret->dest_full_path == NULL) {
        LOG(DCOPY_LOG_ERR, "Failed to allocate full destination path.");
        DCOPY_abort(EXIT_FAILURE);
    }

    return ret;
}

/* given the address of a pointer to an operation_t struct,
 * free associated memory and set pointer to NULL */
void DCOPY_opt_free(DCOPY_operation_t** optptr)
{
    if (optptr != NULL) {
        /* get pointer to operation_t struct */
        DCOPY_operation_t* opt = (*optptr);
        if (opt != NULL) {
            /* free memory and then the object itself */
            free(opt->dest_full_path);
            free(opt);
        }

        /* set caller's pointer to NULL to catch bugs */
        *optptr = NULL;
    }
    return;
}

/**
 * The initial seeding callback for items to process on the distributed queue
 * structure. We send all of our source items to the queue here.
 */
void DCOPY_add_objects(CIRCLE_handle* handle)
{
    DCOPY_enqueue_work_objects(handle);
}

/**
 * The process callback for items found on the distributed queue structure.
 */
void DCOPY_process_objects(CIRCLE_handle* handle)
{
    char op[CIRCLE_MAX_STRING_LEN];
    /*
        const char* DCOPY_op_string_table[] = {
            "TREEWALK",
            "COPY",
            "CLEANUP",
            "COMPARE"
        };
    */

    /* Pop an item off the queue */
    handle->dequeue(op);
    DCOPY_operation_t* opt = DCOPY_decode_operation(op);

    /*
        LOG(DCOPY_LOG_DBG, "Performing operation `%s' on operand `%s' (`%d' remain on local queue).", \
            DCOPY_op_string_table[opt->code], opt->operand, handle->local_queue_size());
    */

    DCOPY_jump_table[opt->code](opt, handle);

    DCOPY_opt_free(&opt);
    return;
}

/* Unlink the destination file. */
void DCOPY_unlink_destination(DCOPY_operation_t* op)
{
    char dest_path_recursive[PATH_MAX];
    char dest_path_file_to_file[PATH_MAX];

    if(op->dest_base_appendix == NULL) {
        sprintf(dest_path_recursive, "%s/%s", \
                DCOPY_user_opts.dest_path, \
                op->operand + op->source_base_offset + 1);

        strncpy(dest_path_file_to_file, DCOPY_user_opts.dest_path, PATH_MAX);
    }
    else {
        sprintf(dest_path_recursive, "%s/%s/%s", \
                DCOPY_user_opts.dest_path, \
                op->dest_base_appendix, \
                op->operand + op->source_base_offset + 1);

        sprintf(dest_path_file_to_file, "%s/%s", \
                DCOPY_user_opts.dest_path, \
                op->dest_base_appendix);
    }

    if(unlink(dest_path_recursive) < 0) {
        LOG(DCOPY_LOG_DBG, "Failed to unlink recursive style destination. " \
            "%s", strerror(errno));
    }

    if(unlink(dest_path_file_to_file) < 0) {
        LOG(DCOPY_LOG_DBG, "Failed to unlink file-to-file style destination. " \
            "%s", strerror(errno));
    }

    return;
}

/* Open the input file as a stream. */
FILE* DCOPY_open_input_stream(DCOPY_operation_t* op)
{
    FILE* in_ptr = fopen64(op->operand, "rb");

    if(in_ptr == NULL) {
        LOG(DCOPY_LOG_DBG, "Failed to open input file `%s'. %s", \
            op->operand, strerror(errno));
        /* Handle operation requeue in parent function. */
    }

    return in_ptr;
}

/* Open the input file as an fd. */
int DCOPY_open_input_fd(DCOPY_operation_t* op, \
                        off64_t offset, \
                        off64_t len)
{
    int in_fd = open64(op->operand, O_RDONLY | O_NOATIME);

    if(in_fd < 0) {
        LOG(DCOPY_LOG_DBG, "Failed to open input file `%s'. %s", \
            op->operand, strerror(errno));
        /* Handle operation requeue in parent function. */
    }

    posix_fadvise64(in_fd, offset, len, POSIX_FADV_SEQUENTIAL);
    return in_fd;
}

/*
 * Open the output file and return a stream.
 *
 * This function needs figure out if this is a file-to-file copy or a
 * recursive copy, then return a FILE* based on the result.
 */
FILE* DCOPY_open_output_stream(DCOPY_operation_t* op)
{
    char dest_path_recursive[PATH_MAX];
    char dest_path_file_to_file[PATH_MAX];

    FILE* out_ptr = NULL;

    if(op->dest_base_appendix == NULL) {
        sprintf(dest_path_recursive, "%s/%s", \
                DCOPY_user_opts.dest_path, \
                op->operand + op->source_base_offset + 1);

        strncpy(dest_path_file_to_file, DCOPY_user_opts.dest_path, PATH_MAX);
    }
    else {
        sprintf(dest_path_recursive, "%s/%s/%s", \
                DCOPY_user_opts.dest_path, \
                op->dest_base_appendix, \
                op->operand + op->source_base_offset + 1);

        sprintf(dest_path_file_to_file, "%s/%s", \
                DCOPY_user_opts.dest_path, \
                op->dest_base_appendix);
    }

    /*
        LOG(DCOPY_LOG_DBG, "Opening destination path `%s' (recursive).", \
            dest_path_recursive);
    */

    /*
     * If we're recursive, we'll be doing this again and again, so try
     * recursive first. If it fails, then do the file-to-file.
     */
    if((out_ptr = fopen64(dest_path_recursive, "rb")) == NULL) {

        /*
                LOG(DCOPY_LOG_DBG, "Opening destination path `%s' " \
                    "(file-to-file fallback).", \
                    dest_path_file_to_file);
        */

        out_ptr = fopen64(dest_path_file_to_file, "rb");
    }

    if(out_ptr == NULL) {
        LOG(DCOPY_LOG_DBG, "Failed to open destination path when comparing " \
            "from source `%s'. %s", op->operand, strerror(errno));

        /* Handle operation requeue in parent function. */
    }

    return out_ptr;
}

/*
 * Open the output file and return a descriptor.
 *
 * This function needs figure out if this is a file-to-file copy or a
 * recursive copy, then return an fd based on the result. The treewalk
 * stage has already setup a directory structure for us to use.
 */
int DCOPY_open_output_fd(DCOPY_operation_t* op)
{
    char dest_path_recursive[PATH_MAX];
    char dest_path_file_to_file[PATH_MAX];

    int out_fd = -1;

    if(op->dest_base_appendix == NULL) {
        sprintf(dest_path_recursive, "%s/%s", \
                DCOPY_user_opts.dest_path, \
                op->operand + op->source_base_offset + 1);

        strncpy(dest_path_file_to_file, DCOPY_user_opts.dest_path, PATH_MAX);
    }
    else {
        sprintf(dest_path_recursive, "%s/%s/%s", \
                DCOPY_user_opts.dest_path, \
                op->dest_base_appendix, \
                op->operand + op->source_base_offset + 1);

        sprintf(dest_path_file_to_file, "%s/%s", \
                DCOPY_user_opts.dest_path, \
                op->dest_base_appendix);
    }

    /*
        LOG(DCOPY_LOG_DBG, "Opening destination path `%s' (recursive).", \
            dest_path_recursive);
    */

    /*
     * If we're recursive, we'll be doing this again and again, so try
     * recursive first. If it fails, then do the file-to-file.
     */
    if((out_fd = open64(dest_path_recursive, O_WRONLY | O_CREAT | O_NOATIME, S_IRWXU)) < 0) {
        /*
                LOG(DCOPY_LOG_DBG, "Opening destination path `%s' " \
                    "(file-to-file fallback).", \
                    dest_path_file_to_file);
        */

        out_fd = open64(dest_path_file_to_file, O_WRONLY | O_CREAT | O_NOATIME, S_IRWXU);
    }

    if(out_fd < 0) {
        LOG(DCOPY_LOG_DBG, "Failed to open destination path when copying " \
            "from source `%s'. %s", op->operand, strerror(errno));

        /* Handle operation requeue in parent function. */
    }

    return out_fd;
}

void DCOPY_copy_xattrs(
    DCOPY_operation_t* op,
    const struct stat64* statbuf,
    const char* dest_path)
{
    char* src_path = op->operand;

    /* copy extended attributes */
    /* allocate space for list_size names */
    size_t list_bufsize = 0;
    char* list = NULL;

    /* get list, if list_size == ERANGE, try again */
    ssize_t list_size;
    int got_list = 0;
    while (! got_list) {
        list_size = llistxattr(src_path, list, list_bufsize);
        if (list_size < 0) {
            if (errno == ERANGE) {
                /* buffer is too small, allocate size specified in list_size */
                if (list != NULL) {
                    free(list);
                    list = NULL;
                    list_bufsize = 0;
                }
                list_bufsize = (size_t) list_size;
                if (list_bufsize > 0) {
                    list = (char*) malloc(list_bufsize);
                    if (list == NULL) {
                        /* ERROR */
                    }
                }
            } else if (errno == ENOTSUP) {
                /* this is common enough that we silently ignore it */
                break;
            } else {
                /* this is a real error */
                LOG(DCOPY_LOG_ERR, "Failed to get list of extended attributes on %s llistxattr() errno=%d %s.",
                    src_path, errno, strerror(errno)
                );
                break;
            }
        } else if (list_size > 0) {
            /* got our list */
            got_list = 1;
        } else {
            /* list_size == 0, symlinks seem to return this */
            break;
        }
    }

    /* iterate over list and copy values to new object lgetxattr/lsetxattr */
    if (got_list) {
        char* name = list;
        while (name < list + list_size) {
            /* allocate a string to hold value */
            size_t val_bufsize = 0;
            void* val = NULL;

            /* lookup value for name */
            int got_val = 0;
            while (! got_val) {
                ssize_t val_size = lgetxattr(src_path, name, val, val_bufsize);
                if (val_size < 0) {
                    if (errno == ERANGE) {
                        /* buffer is too small, allocate a buffer for val_size */
                        if (val != NULL) {
                            free(val);
                            val = NULL;
                            val_bufsize = 0;
                        }
                        val_bufsize = (size_t) val_size;
                        if (val_bufsize > 0) {
                            val = malloc(val_bufsize);
                            if (val == NULL) {
                                /* ERROR */
                            }
                        }
                    } else if (errno == ENOATTR) {
                        /* source object no longer has this attribute,
                         * maybe deleted out from under us */
                        break;
                    } else {
                        LOG(DCOPY_LOG_ERR, "Failed to get value for name=%s on %s llistxattr() errno=%d %s.",
                           name, src_path, errno, strerror(errno)
                        );
                        break;
                    }
                } else {
                    got_val = 1;
                }
            }

            /* set attribute on destination object */
            if (got_val) {
                int setrc = lsetxattr(dest_path, name, val, val_bufsize, 0);
                if (setrc != 0) {
                    LOG(DCOPY_LOG_ERR, "Failed to set value for name=%s on %s llistxattr() errno=%d %s.",
                       name, dest_path, errno, strerror(errno)
                    );
                }
            }

            /* free value string */
            if (val != NULL) {
                free(val);
                val = NULL;
                val_bufsize = 0;
            }

            /* jump to next name */
            size_t namelen = strlen(name) + 1;
            name += namelen;
        }
    }

    /* free space allocated for list */
    if (list != NULL) {
        free(list);
        list = NULL;
        list_bufsize = 0;
    }

    return;
}

void DCOPY_copy_ownership(
    DCOPY_operation_t* op,
    const struct stat64* statbuf,
    const char* dest_path)
{
    /* note that we use lchown to change ownership of link itself, it path happens to be a link */
    if (lchown(dest_path, statbuf->st_uid, statbuf->st_gid) != 0) {
        LOG(DCOPY_LOG_ERR, "Failed to change ownership on %s lchown() errno=%d %s.",
            dest_path, errno, strerror(errno)
        );
    }

    return;
}

void DCOPY_copy_permissions(
    DCOPY_operation_t* op,
    const struct stat64* statbuf,
    const char* dest_path)
{
    /* change mode */
    if (! S_ISLNK(statbuf->st_mode)) {
        if (chmod(dest_path, statbuf->st_mode) != 0) {
            LOG(DCOPY_LOG_ERR, "Failed to change permissions on %s chmod() errno=%d %s.",
                dest_path, errno, strerror(errno)
            );
        }
    }

    return;
}

void DCOPY_copy_timestamps(
    DCOPY_operation_t* op,
    const struct stat64* statbuf,
    const char* dest_path)
{
    /* TODO: see stat-time.h and get_stat_atime/mtime/ctime to read sub-second times,
     * and use utimensat to set sub-second times */
    /* as last step, change timestamps */
    if (! S_ISLNK(statbuf->st_mode)) {
        struct utimbuf times;
        times.actime  = statbuf->st_atime;
        times.modtime = statbuf->st_mtime;
        if (utime(dest_path, &times) != 0) {
            LOG(DCOPY_LOG_ERR, "Failed to change timestamps on %s utime() errno=%d %s.",
              dest_path, errno, strerror(errno)
            );
        }
    } else {
        struct timeval tv[2];
        tv[0].tv_sec  = statbuf->st_atime;
        tv[0].tv_usec = 0;
        tv[1].tv_sec  = statbuf->st_mtime;
        tv[1].tv_usec = 0;
        if (lutimes(dest_path, tv) != 0) {
            LOG(DCOPY_LOG_ERR, "Failed to change timestamps on %s utime() errno=%d %s.",
              dest_path, errno, strerror(errno)
            );
        }
    }

    return;
}

/* called by single process upon detection of a problem */
void DCOPY_abort(int code)
{
    // MPI_Abort(MPI_COMM_WORLD, code);
    exit(code);
}

/* called globally by all procs to exit */
void DCOPY_exit(int code)
{
    // MPI_Finalize();
    exit(code);
}

/* EOF */
