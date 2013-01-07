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

/* Open the input file. */
FILE* DCOPY_open_input_file(DCOPY_operation_t* op)
{
    FILE* in_ptr = fopen(op->operand, "rb");

    if(in_ptr == NULL) {
        LOG(DCOPY_LOG_DBG, "Failed to open input file `%s'. %s", \
            op->operand, strerror(errno));
        /* Handle operation requeue in parent function. */
    }

    return in_ptr;
}

/*
 * Open the output file.
 *
 * This function needs figure out if this is a file-to-file copy or a
 * recursive copy, then return an fd based on the result. The treewalk
 * stage has already setup a directory structure for us to use.
 */
int DCOPY_open_output_file(DCOPY_operation_t* op)
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

    LOG(DCOPY_LOG_DBG, "Opening destination path `%s' (recursive).", \
        dest_path_recursive);

    /*
     * If we're recursive, we'll be doing this again and again, so try
     * recursive first. If it fails, then do the file-to-file.
     */
    if((out_fd = open(dest_path_recursive, O_RDWR | O_CREAT, 00644)) < 0) {

        LOG(DCOPY_LOG_DBG, "Opening destination path `%s' " \
            "(file-to-file fallback).", \
            dest_path_file_to_file);

        out_fd = open(dest_path_file_to_file, O_RDWR | O_CREAT, 00644);
    }

    if(out_fd < 0) {
        LOG(DCOPY_LOG_DBG, "Failed to open destination path when copying " \
                           "from source `%s'.", op->operand);

        /* Handle operation requeue in parent function. */
    }

    return out_fd;
}

/*
 * Perform the actual copy on this chunk and increment the global statistics
 * counter.
 */
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

        /* Handle operation requeue in parent function. */
        return -1;
    }

    num_of_bytes_read = fread((void*)io_buf, 1, DCOPY_CHUNK_SIZE, in_ptr);

    if(num_of_bytes_read <= 0) {
        LOG(DCOPY_LOG_ERR, "Couldn't read from source path `%s'. %s", \
            op->operand, strerror(errno));

        /* Handle operation requeue in parent function. */
        return -1;
    }

    LOG(DCOPY_LOG_DBG, "Read `%zu' bytes at offset `%d'.", num_of_bytes_read, \
        DCOPY_CHUNK_SIZE * op->chunk);

    if(lseek(out_fd, DCOPY_CHUNK_SIZE * op->chunk, SEEK_SET) < 0) {
        LOG(DCOPY_LOG_ERR, "Couldn't seek in destination path (source is `%s'). %s", \
            op->operand, strerror(errno));

        /* Handle operation requeue in parent function. */
        return -1;
    }

    num_of_bytes_written = write(out_fd, io_buf, num_of_bytes_read);

    if(num_of_bytes_written < 0) {
        LOG(DCOPY_LOG_ERR, "Write error when copying from `%s'. %s", \
            op->operand, strerror(errno));

        /* Handle operation requeue in parent function. */
        return -1;
    }

    /* Increment the global counter. */
    DCOPY_statistics.total_bytes_copied += (uint64_t) num_of_bytes_written;

    LOG(DCOPY_LOG_DBG, "Wrote %zu bytes at offset `%d' (%zu total).", \
        num_of_bytes_written, DCOPY_CHUNK_SIZE * op->chunk, \
        DCOPY_statistics.total_bytes_copied);

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
