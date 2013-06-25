/*
 * This file contains the logic to truncate the the destination as well as
 * preserve permissions and ownership. Since it would be redundant, we only
 * pay attention to the first chunk of each file and pass the rest along.

 * See the file "COPYING" for the full license governing this code.
 */

#include "cleanup.h"
#include "dcp.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utime.h>

/** Options specified by the user. */
extern DCOPY_options_t DCOPY_user_opts;

/*
 * Preserve the permissions. Preserve the setuid and setgid bits if the
 * preserve_setxid flag is true. On failure, return false.
 */
bool DCOPY_set_preserve_permissions(DCOPY_operation_t* op, \
                                    CIRCLE_handle* handle,
                                    bool preserve_setxid,
                                    const struct stat64* statbuf,
                                    const char* dest_path)
{
    if(chmod(dest_path, statbuf->st_mode) != 0) {
        LOG(DCOPY_LOG_ERR, "Failed to change permissions on %s chmod() errno=%d %s.", dest_path, errno, strerror(errno));
    }

    /* TODO: preserve permissions, requeue to cleanup if fail and unreliable. */

    return false;
}

/* Preserve the timestamp of last modification and time of last access. On
 * failure, return false.
 */
bool DCOPY_set_preserve_timestamps(DCOPY_operation_t* op, \
                                   CIRCLE_handle* handle,
                                   const struct stat64* statbuf,
                                   const char* dest_path)
{
    struct utimbuf times;
    times.actime  = statbuf->st_atime;
    times.modtime = statbuf->st_mtime;

    if(utime(dest_path, &times) != 0) {
        LOG(DCOPY_LOG_ERR, "Failed to change timestamps on %s utime() errno=%d %s.", dest_path, errno, strerror(errno));
    }

    /* TODO: preserve timestamps, requeue to cleanup if fail and unreliable. */

    return false;
}

/* Attempt to preserve the owner and group. On failure, return false. */
bool DCOPY_set_preserve_ownership(DCOPY_operation_t* op, \
                                  CIRCLE_handle* handle,
                                  const struct stat64* statbuf,
                                  const char* dest_path)
{
    /* note that we use lchown to change ownership of link itself, it path happens to be a link */
    if(lchown(dest_path, statbuf->st_uid, statbuf->st_gid) != 0) {
        LOG(DCOPY_LOG_ERR, "Failed to change ownership on %s chown() errno=%d %s.", dest_path, errno, strerror(errno));
    }

    /* TODO: preserve ownership, requeue to cleanup if fail and unreliable. */

    return false;
}

void DCOPY_truncate_file(DCOPY_operation_t* op, \
                         CIRCLE_handle* handle)
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

    LOG(DCOPY_LOG_DBG, "Truncating file to `%" PRId64 "'.", op->file_size);

    /*
     * Try the recursive file before file-to-file. The cast below requires us
     * to have a maximum file_size of 2^63, not 2^64.
     */
    if(truncate64(dest_path_recursive, op->file_size) < 0) {
        if(truncate64(dest_path_file_to_file, op->file_size) < 0) {
            LOG(DCOPY_LOG_ERR, "Failed to truncate destination file: %s (errno=%d %s)",
                dest_path_recursive, errno, strerror(errno));

            DCOPY_retry_failed_operation(COPY, handle, op);
            return;
        }
    }
}

void DCOPY_do_cleanup(DCOPY_operation_t* op, \
                      CIRCLE_handle* handle)
{
    char* newop;
    bool ownership_preserved;

    /*
     * Only bother truncating and setting permissions on the first chunk of
     * the file.
     */
    if(op->chunk == 0) {

        /* truncate file to appropriate size, to do this before
         * setting permissions in case file does not have write permission */
        DCOPY_truncate_file(op, handle);

        /* set file timestamps and permissions */
        if(DCOPY_user_opts.preserve) {
            /* build destination object name */
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

            /* get stat of source object */
            struct stat64 statbuf;

            /* TODO: would be nice to do this without needing another call to stat */
            /* setting attributes in this order seems to work, but I wonder if there
             * are systems where this order should be changed */
            if(lstat64(op->operand, &statbuf) == 0) {
                /* set owner and group */
                ownership_preserved = DCOPY_set_preserve_ownership(op, handle, &statbuf, dest_path_recursive);

                /* set read/write/execute bits */
                DCOPY_set_preserve_permissions(op, handle, ownership_preserved, &statbuf, dest_path_recursive);

                /* set timestamps */
                DCOPY_set_preserve_timestamps(op, handle, &statbuf, dest_path_recursive);
            }
            else {
                /*
                        LOG(DCOPY_LOG_ERR, "Could not determine if `%s' is a directory. %s", path, strerror(errno));
                */
            }
        }
    }

    /*
     * If the user is feeling brave, this is where we let them skip the
     * comparison stage.
     */
    if(!DCOPY_user_opts.skip_compare) {
        newop = DCOPY_encode_operation(COMPARE, op->chunk, op->operand, \
                                       op->source_base_offset, \
                                       op->dest_base_appendix, op->file_size);

        handle->enqueue(newop);
        free(newop);
    }

    return;
}

/* EOF */
