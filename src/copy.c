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
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/** Options specified by the user. */
extern DCOPY_options_t DCOPY_user_opts;

/** Statistics to gather for summary output. */
extern DCOPY_statistics_t DCOPY_statistics;

/* The entrance point to the copy operation. */
void DCOPY_do_copy(DCOPY_operation_t* op, \
                   CIRCLE_handle* handle)
{
    FILE* in_ptr = DCOPY_open_input_file(op);
    if(in_ptr == NULL) {
        DCOPY_retry_failed_operation(COPY, handle, op);
        return;
    }

    int out_fd = DCOPY_open_output_file(op);
    if(out_fd < 0) {
        DCOPY_retry_failed_operation(COPY, handle, op);
        return;
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

FILE* DCOPY_open_input_file(DCOPY_operation_t* op)
{
    FILE* in_ptr = fopen(op->operand, "rb");

    if(in_ptr == NULL) {
        LOG(DCOPY_LOG_DBG, "Failed to open input file `%s'. %s", \
            op->operand, strerror(errno));
    }

    return in_ptr;
}

int DCOPY_open_output_file(DCOPY_operation_t* op)
{
    char dest_path[PATH_MAX];

    if(op->dest_base_appendix == NULL) {
        sprintf(dest_path, "%s/%s", \
                DCOPY_user_opts.dest_path, \
                op->operand + op->source_base_offset + 1);
    }
    else {
        sprintf(dest_path, "%s/%s/%s", \
                DCOPY_user_opts.dest_path, \
                op->dest_base_appendix, \
                op->operand + op->source_base_offset + 1);
    }

    return open(dest_path, O_RDWR | O_CREAT, 00644);
}

int DCOPY_perform_copy(DCOPY_operation_t* op, \
                       FILE* in_ptr, \
                       int out_fd)
{
    char io_buf[DCOPY_CHUNK_SIZE];

    size_t num_of_bytes_read = 0;
    ssize_t num_of_bytes_written = 0;

    if(fseek(in_ptr, DCOPY_CHUNK_SIZE * op->chunk, SEEK_SET) != 0) {
        LOG(DCOPY_LOG_ERR, "Couldn't seek in source path `%s'. %s", \
            op->operand, strerror(errno));
        return -1;
    }

    num_of_bytes_read = fread((void*)io_buf, 1, DCOPY_CHUNK_SIZE, in_ptr);

    if(num_of_bytes_read <= 0) {
        LOG(DCOPY_LOG_ERR, "Couldn't read from source path `%s'. %s", \
            op->operand, strerror(errno));
        return -1;
    }

    LOG(DCOPY_LOG_DBG, "Read `%zu' bytes at offset `%d'.", num_of_bytes_read, \
        DCOPY_CHUNK_SIZE * op->chunk);

    if(lseek(out_fd, DCOPY_CHUNK_SIZE * op->chunk, SEEK_SET) < 0) {
        LOG(DCOPY_LOG_ERR, "Couldn't seek in destination path (source is `%s'). %s", \
            op->operand, strerror(errno));
        return -1;
    }

    num_of_bytes_written = write(out_fd, io_buf, num_of_bytes_read);

    if(num_of_bytes_written < 0) {
        LOG(DCOPY_LOG_ERR, "Write error when copying from `%s'. %s", \
            op->operand, strerror(errno));
        return -1;
    }

    /* Increment the global counter. */
    DCOPY_statistics.total_bytes_copied += (uint64_t) num_of_bytes_written;

    LOG(DCOPY_LOG_DBG, "Wrote %zu bytes at offset `%d' (%zu total).", \
        num_of_bytes_written, DCOPY_CHUNK_SIZE * op->chunk, \
        DCOPY_statistics.total_bytes_copied);

    return 1;
}

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
