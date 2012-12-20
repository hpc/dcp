/* See the file "COPYING" for the full license governing this code. */

#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "compare.h"

/** Options specified by the user. */
extern DCOPY_options_t DCOPY_user_opts;

/** The loglevel that this instance of dcopy will output. */
extern DCOPY_loglevel  DCOPY_debug_level;

void DCOPY_do_compare(DCOPY_operation_t* op, CIRCLE_handle* handle)
{
    FILE* src_stream;
    FILE* dest_stream;

    size_t src_bytes;
    size_t dest_bytes;

    char* newop;

    char dest_path[PATH_MAX];
    char* src_path = op->operand;

    void* src_buf = (void*) malloc(DCOPY_CHUNK_SIZE);
    void* dest_buf = (void*) malloc(DCOPY_CHUNK_SIZE);

    if(op->dest_base_appendix == NULL) {
        sprintf(dest_path, "%s/%s", DCOPY_user_opts.dest_path, src_path + op->source_base_offset);
    }
    else {
        sprintf(dest_path, "%s/%s/%s", DCOPY_user_opts.dest_path, op->dest_base_appendix, src_path + op->source_base_offset);
    }

    LOG(DCOPY_LOG_DBG, "Comparing source object `%s' (chunk number `%d') against destination `%s'.", \
        src_path, op->chunk, dest_path);

    src_stream = fopen(src_path, "rb");
    dest_stream = fopen(dest_path, "rb");

    if(!src_stream) {
        LOG(DCOPY_LOG_ERR, "Compare stage is unable to open source file `%s'. %s", \
            src_path, strerror(errno));

        /** FIXME: remove this exit once the compare stage works well. */
        exit(EXIT_FAILURE);

        return;
    }

    if(!dest_stream) {
        LOG(DCOPY_LOG_ERR, "Compare stage is unable to open destination file `%s'. %s", \
            dest_path, strerror(errno));

        /** FIXME: remove this exit once the compare stage works well. */
        exit(EXIT_FAILURE);

        /* FIXME: add a flag to turn off retry. */
        newop = DCOPY_encode_operation(COMPARE, op->chunk, src_path, op->source_base_offset, op->dest_base_appendix);
        handle->enqueue(newop);

        free(newop);

        return;
    }

    fseek(src_stream, DCOPY_CHUNK_SIZE * op->chunk, SEEK_SET);
    fseek(dest_stream, DCOPY_CHUNK_SIZE * op->chunk, SEEK_SET);

    src_bytes = fread(src_buf, 1, DCOPY_CHUNK_SIZE, src_stream);
    dest_bytes = fread(dest_buf, 1, DCOPY_CHUNK_SIZE, dest_stream);

    if(src_bytes != dest_bytes || memcmp(src_buf, dest_buf, src_bytes) != 0) {
        LOG(DCOPY_LOG_ERR, "Compare mismatch! Requeueing file `%s'.", src_path);

        /** FIXME: remove this exit once the compare stage works well. */
        exit(EXIT_FAILURE);

        /* FIXME: add a flag to turn off retry. */
        newop = DCOPY_encode_operation(STAT, 0, src_path, op->source_base_offset, op->dest_base_appendix);
        handle->enqueue(newop);

        free(newop);
    }
    else {
        LOG(DCOPY_LOG_DBG, "File `%s' (chunk `%d') compare successful.", src_path, op->chunk);
    }

    fclose(src_stream);
    fclose(dest_stream);

    free(src_buf);
    free(dest_buf);

    return;
}

/* EOF */
