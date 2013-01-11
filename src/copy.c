/* See the file "COPYING" for the full license governing this code. */

#include "copy.h"
#include "treewalk.h"
#include "dcp.h"

#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <inttypes.h>
// #include <sys/sendfile.h>

/** Options specified by the user. */
extern DCOPY_options_t DCOPY_user_opts;

/** Statistics to gather for summary output. */
extern DCOPY_statistics_t DCOPY_statistics;

/* The entrance point to the copy operation. */
void DCOPY_do_copy(DCOPY_operation_t* op, \
                   CIRCLE_handle* handle)
{
    int in_fd = DCOPY_open_input_fd(op);

    if(in_fd < 0) {
        DCOPY_retry_failed_operation(COPY, handle, op);
        return;
    }

    int out_fd = DCOPY_open_output_fd(op);

    if(out_fd < 0) {
        /*
         * If the force option is specified, try to unlink the destination and
         * reopen before doing the optional requeue.
         */
        if(DCOPY_user_opts.force) {
            DCOPY_unlink_destination(op);
            out_fd = DCOPY_open_output_fd(op);

            if(out_fd < 0) {
                DCOPY_retry_failed_operation(COPY, handle, op);
                return;
            }
        }
        else {
            DCOPY_retry_failed_operation(COPY, handle, op);
            return;
        }
    }

    if(DCOPY_perform_copy(op, in_fd, out_fd) < 0) {
        DCOPY_retry_failed_operation(COPY, handle, op);
        return;
    }

    if(close(in_fd) < 0) {
        LOG(DCOPY_LOG_DBG, "Close on source file failed. %s", strerror(errno));
    }

    if(close(out_fd) < 0) {
        LOG(DCOPY_LOG_DBG, "Close on destination file failed. %s", strerror(errno));
    }

    DCOPY_enqueue_cleanup_stage(op, handle);

    return;
}

/*
 * Perform the actual copy on this chunk and increment the global statistics
 * counter.
 */
int DCOPY_perform_copy(DCOPY_operation_t* op, \
                       int in_fd, \
                       int out_fd)
{
    off64_t offset = DCOPY_CHUNK_SIZE * op->chunk;
    ssize_t num_of_bytes_read = 0;
    ssize_t num_of_bytes_written = 0;
    ssize_t total_bytes_written = 0;

    char io_buf[FD_PAGE_CACHE_SIZE];

    if(lseek64(in_fd, offset, SEEK_SET) < 0) {
        LOG(DCOPY_LOG_ERR, "Couldn't seek in source path `%s'. %s", \
            op->operand, strerror(errno));
        /* Handle operation requeue in parent function. */
        return -1;
    }

    if(lseek64(out_fd, offset, SEEK_SET) < 0) {
        LOG(DCOPY_LOG_ERR, "Couldn't seek in destination path (source is `%s'). %s", \
            op->operand, strerror(errno));
        return -1;
    }

    while(total_bytes_written <= DCOPY_CHUNK_SIZE) {

        num_of_bytes_read = read(in_fd, &io_buf[0], FD_PAGE_CACHE_SIZE);

        if(!num_of_bytes_read) {
            break;
        }

        num_of_bytes_written = write(out_fd, &io_buf[0], \
                                     num_of_bytes_read);

        if(num_of_bytes_written != num_of_bytes_read) {
            LOG(DCOPY_LOG_ERR, "Write error when copying from `%s'. %s", \
                op->operand, strerror(errno));
            return -1;
        }

        total_bytes_written += num_of_bytes_written;
    }

    /* Increment the global counter. */
    DCOPY_statistics.total_bytes_copied += total_bytes_written;

/*
    LOG(DCOPY_LOG_DBG, "Wrote `%zu' bytes at segment `%" PRId64 \
        "', offset `%" PRId64 "' (`%" PRId64 "' total).", \
        num_of_bytes_written, op->chunk, DCOPY_CHUNK_SIZE * op->chunk, \
        DCOPY_statistics.total_bytes_copied);
*/

    return 1;
}

/*
 * Encode and enqueue the cleanup stage for this chunk so the file is
 * truncated and (if specified via getopt) permissions are preserved.
 */
void DCOPY_enqueue_cleanup_stage(DCOPY_operation_t* op, \
                                 CIRCLE_handle* handle)
{
    char* newop;

    newop = DCOPY_encode_operation(CLEANUP, op->chunk, op->operand, \
                                   op->source_base_offset, \
                                   op->dest_base_appendix, op->file_size);

    handle->enqueue(newop);
    free(newop);
}

/* EOF */
