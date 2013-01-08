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

/** Options specified by the user. */
extern DCOPY_options_t DCOPY_user_opts;

/** Statistics to gather for summary output. */
extern DCOPY_statistics_t DCOPY_statistics;

/* The entrance point to the copy operation. */
void DCOPY_do_copy(DCOPY_operation_t* op, \
                   CIRCLE_handle* handle)
{
    FILE* in_ptr = DCOPY_open_input_stream(op);

    if(in_ptr == NULL) {
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

    if(DCOPY_perform_copy(op, in_ptr, out_fd) < 0) {
        DCOPY_retry_failed_operation(COPY, handle, op);
        return;
    }

    if(fclose(in_ptr) < 0) {
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
                       FILE* in_ptr, \
                       int out_fd)
{
    char* io_buf = (char*) malloc(sizeof(char) * DCOPY_CHUNK_SIZE);

    size_t num_of_bytes_read = 0;
    ssize_t num_of_bytes_written = 0;

    if(fseeko64(in_ptr, (int64_t)DCOPY_CHUNK_SIZE * (int64_t)op->chunk, SEEK_SET) != 0) {
        LOG(DCOPY_LOG_ERR, "Couldn't seek in source path `%s'. %s", \
            op->operand, strerror(errno));

        free(io_buf);
        /* Handle operation requeue in parent function. */
        return -1;
    }

    num_of_bytes_read = fread(io_buf, 1, DCOPY_CHUNK_SIZE, in_ptr);

/*
    LOG(DCOPY_LOG_DBG, "Number of bytes read is `%zu'.", num_of_bytes_read);
*/

    if(num_of_bytes_read <= 0 && op->file_size > 0) {
        LOG(DCOPY_LOG_ERR, "Couldn't read from source path `%s'. %s", \
            op->operand, strerror(errno));

        free(io_buf);
        /* Handle operation requeue in parent function. */
        return -1;
    }

/*
    LOG(DCOPY_LOG_DBG, "Read `%zu' bytes at offset `%zu'.", num_of_bytes_read, \
        (int64_t)DCOPY_CHUNK_SIZE * (int64_t)op->chunk);
*/

    if(lseek64(out_fd, (int64_t)DCOPY_CHUNK_SIZE * (int64_t)op->chunk, SEEK_SET) < 0) {
        LOG(DCOPY_LOG_ERR, "Couldn't seek in destination path (source is `%s'). %s", \
            op->operand, strerror(errno));

        free(io_buf);
        /* Handle operation requeue in parent function. */
        return -1;
    }

    num_of_bytes_written = write(out_fd, io_buf, num_of_bytes_read);

    if(num_of_bytes_written < 0 && op->file_size > 0) {
        LOG(DCOPY_LOG_ERR, "Write error when copying from `%s'. %s", \
            op->operand, strerror(errno));

        free(io_buf);
        /* Handle operation requeue in parent function. */
        return -1;
    }

    /* Increment the global counter. */
    DCOPY_statistics.total_bytes_copied += num_of_bytes_written;

    LOG(DCOPY_LOG_DBG, "Wrote `%zu' bytes at offset `%" PRId64 "' (`%" PRId64 "' total).", \
        num_of_bytes_written, (int64_t)DCOPY_CHUNK_SIZE * (int64_t)op->chunk, \
        DCOPY_statistics.total_bytes_copied);

    free(io_buf);
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
