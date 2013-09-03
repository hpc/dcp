/* See the file "COPYING" for the full license governing this code. */

#include "compare.h"
#include "dcp.h"

#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/** Options specified by the user. */
extern DCOPY_options_t DCOPY_user_opts;

/** Statistics to gather for summary output. */
extern DCOPY_statistics_t DCOPY_statistics;

/*
 * Perform the compare on this chunk.
 */
static int DCOPY_perform_compare(DCOPY_operation_t* op,
                          int in_fd,
                          int out_fd,
                          off64_t offset)
{
    if(lseek64(in_fd, offset, SEEK_SET) < 0) {
        LOG(DCOPY_LOG_ERR, "Couldn't seek in source path `%s'. errno=%d %s", \
            op->operand, errno, strerror(errno));
        /* Handle operation requeue in parent function. */
        return -1;
    }

    if(lseek64(out_fd, offset, SEEK_SET) < 0) {
        LOG(DCOPY_LOG_ERR, "Couldn't seek in destination path (source is `%s'). errno=%d %s", \
            op->operand, errno, strerror(errno));
        return -1;
    }

    /* get buffer info */
    size_t buf_size = DCOPY_user_opts.block_size;
    void* src_buf = DCOPY_user_opts.block_buf1;
    void* dest_buf = DCOPY_user_opts.block_buf2;

    size_t num_of_in_bytes = 0;
    size_t num_of_out_bytes = 0;
    size_t total_bytes = 0;

    size_t chunk_size = DCOPY_user_opts.chunk_size;
    while(total_bytes <= chunk_size) {
        size_t left_to_read = chunk_size - total_bytes;
        if (left_to_read > buf_size) {
            left_to_read = buf_size;
        }

        num_of_in_bytes = read(in_fd, src_buf, left_to_read);
        num_of_out_bytes = read(out_fd, dest_buf, left_to_read);

        if(num_of_in_bytes != num_of_out_bytes) {
            LOG(DCOPY_LOG_DBG, "Source byte count `%zu' does not match " \
                "destination byte count '%zu' of total file size `%zu'.", \
                num_of_in_bytes, num_of_out_bytes, op->file_size);

            return -1;
        }

        if(!num_of_in_bytes) {
            break;
        }

        if(memcmp(src_buf, dest_buf, num_of_in_bytes) != 0) {
            LOG(DCOPY_LOG_ERR, "Compare mismatch when copying from file `%s'.", \
                op->operand);

            return -1;
        }

        total_bytes += num_of_in_bytes;
    }

    return 1;
}

/* The entrance point to the compare operation. */
void DCOPY_do_compare(DCOPY_operation_t* op,
                      CIRCLE_handle* handle)
{
    off64_t offset = DCOPY_user_opts.chunk_size * op->chunk;
    int in_fd = DCOPY_open_input_fd(op, offset, DCOPY_user_opts.chunk_size);

    if(in_fd < 0) {
        DCOPY_retry_failed_operation(COMPARE, handle, op);
        return;
    }

    int out_fd = DCOPY_open_output_for_read_fd(op, offset, DCOPY_user_opts.chunk_size);

    if(out_fd < 0) {
        DCOPY_retry_failed_operation(COMPARE, handle, op);
        return;
    }

    if(DCOPY_perform_compare(op, in_fd, out_fd, offset) < 0) {
        DCOPY_retry_failed_operation(COMPARE, handle, op);
        return;
    }

    if(close(in_fd) < 0) {
        LOG(DCOPY_LOG_DBG, "Close on source file failed. errno=%d %s", errno, strerror(errno));
    }

    if(close(out_fd) < 0) {
        LOG(DCOPY_LOG_DBG, "Close on destination file failed. errno=%d %s", errno, strerror(errno));
    }

    return;
}

/* EOF */
