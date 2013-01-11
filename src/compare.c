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

/* The entrance point to the compare operation. */
void DCOPY_do_compare(DCOPY_operation_t* op, \
                      CIRCLE_handle* handle)
{
    FILE* in_ptr = DCOPY_open_input_stream(op);

    if(in_ptr == NULL) {
        LOG(DCOPY_LOG_DBG, "Failed to open input stream. %s", strerror(errno));
        DCOPY_retry_failed_operation(COPY, handle, op);
        return;
    }

    FILE* out_ptr = DCOPY_open_output_stream(op);

    if(out_ptr == NULL) {
        LOG(DCOPY_LOG_DBG, "Failed to open output stream. %s", strerror(errno));
        DCOPY_retry_failed_operation(COPY, handle, op);
        return;
    }

    if(DCOPY_perform_compare(op, in_ptr, out_ptr) < 0) {
        DCOPY_retry_failed_operation(COPY, handle, op);
        return;
    }

    if(fclose(in_ptr) < 0) {
        LOG(DCOPY_LOG_DBG, "Close on source file failed. %s", strerror(errno));
    }

    if(fclose(out_ptr) < 0) {
        LOG(DCOPY_LOG_DBG, "Close on destination file failed. %s", strerror(errno));
    }

    return;
}

/*
 * Perform the compare on this chunk.
 */
int DCOPY_perform_compare(DCOPY_operation_t* op, \
                          FILE* in_ptr, \
                          FILE* out_ptr)
{
    size_t num_of_in_bytes = 0;
    size_t num_of_out_bytes = 0;

    void* src_buf = (void*) malloc(sizeof(char) * DCOPY_CHUNK_SIZE);
    void* dest_buf = (void*) malloc(sizeof(char) * DCOPY_CHUNK_SIZE);

    fseeko64(in_ptr, (int64_t)DCOPY_CHUNK_SIZE * (int64_t)op->chunk, SEEK_SET);
    fseeko64(out_ptr, DCOPY_CHUNK_SIZE * op->chunk, SEEK_SET);

    num_of_in_bytes = fread(src_buf, 1, DCOPY_CHUNK_SIZE, in_ptr);
    num_of_out_bytes = fread(dest_buf, 1, DCOPY_CHUNK_SIZE, out_ptr);

    if(num_of_in_bytes != num_of_out_bytes) {
        LOG(DCOPY_LOG_DBG, "Source byte count `%zu' does not match " \
            "destination byte count '%zu' of total file size `%zu'.", \
            num_of_in_bytes, num_of_out_bytes, op->file_size);

        free(src_buf);
        free(dest_buf);

        return -1;
    }

    if(memcmp(src_buf, dest_buf, num_of_in_bytes) != 0) {
        LOG(DCOPY_LOG_ERR, "Compare mismatch when copying from file `%s'.", \
            op->operand);

        free(src_buf);
        free(dest_buf);

        return -1;
    }
    else {
        /*
                LOG(DCOPY_LOG_DBG, "File `%s' (chunk `%d') compare successful.", \
                    op->operand, op->chunk);
        */

        free(src_buf);
        free(dest_buf);

        return 1;
    }
}

/* EOF */
