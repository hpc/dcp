/* See the file "COPYING" for the full license governing this code. */

#include "compare.h"
#include "dcp.h"

#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

/** Options specified by the user. */
extern DCOPY_options_t DCOPY_user_opts;

void DCOPY_do_compare(DCOPY_operation_t* op, \
                      CIRCLE_handle* handle)
{
    FILE* src_stream;
    FILE* dest_stream;

    bool is_file_to_file_compare = false;

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

    src_stream = fopen(src_path, "rb");
    dest_stream = fopen(dest_path, "rb");

    if(!src_stream) {
        LOG(DCOPY_LOG_ERR, "Compare stage is unable to open source file `%s'. %s", \
            src_path, strerror(errno));

        if(DCOPY_user_opts.reliable_filesystem) {
            exit(EXIT_FAILURE);
        }
        else {
            /* Retry the entire compare operation. */
            newop = DCOPY_encode_operation(COMPARE, op->chunk, src_path, op->source_base_offset, op->dest_base_appendix, op->file_size);
            handle->enqueue(newop);
            free(newop);

            fclose(src_stream);
            fclose(dest_stream);

            free(src_buf);
            free(dest_buf);

            return;
        }
    }

    if(!dest_stream) {
        /*
         * Since we might be trying a file to file compare, lets try to open
         * the base instead. If it really is a directory, we'll go ahead and
         * fail.
         */
        LOG(DCOPY_LOG_DBG, "Attempting to see if this is a file to file compare.");
        dest_stream = fopen(DCOPY_user_opts.dest_path, "rb");

        if(!dest_stream) {
            LOG(DCOPY_LOG_ERR, "Unable to open destination path `%s'. %s", \
                dest_path, strerror(errno));

            if(DCOPY_user_opts.reliable_filesystem) {
                LOG(DCOPY_LOG_ERR, "Retrying since unreliable filesystem was specified.");
                exit(EXIT_FAILURE);
            }
            else {
                /* Retry the entire compare operation. */
                newop = DCOPY_encode_operation(COMPARE, op->chunk, src_path, op->source_base_offset, op->dest_base_appendix, op->file_size);
                handle->enqueue(newop);
                free(newop);

                fclose(src_stream);
                fclose(dest_stream);

                free(src_buf);
                free(dest_buf);

                return;
            }
        }
        else {
            is_file_to_file_compare = true;
        }
    }

    if(is_file_to_file_compare) {
        LOG(DCOPY_LOG_DBG, "Comparing source object `%s' (chunk number `%d') against destination `%s'.", \
            src_path, op->chunk, DCOPY_user_opts.dest_path);
    }
    else {
        LOG(DCOPY_LOG_DBG, "Comparing source object `%s' (chunk number `%d') against destination `%s'.", \
            src_path, op->chunk, dest_path);
    }

    fseek(src_stream, DCOPY_CHUNK_SIZE * op->chunk, SEEK_SET);
    fseek(dest_stream, DCOPY_CHUNK_SIZE * op->chunk, SEEK_SET);

    src_bytes = fread(src_buf, 1, DCOPY_CHUNK_SIZE, src_stream);
    dest_bytes = fread(dest_buf, 1, DCOPY_CHUNK_SIZE, dest_stream);

    if(src_bytes != dest_bytes) {
        LOG(DCOPY_LOG_DBG, "Source byte count `%zu' does not match destination byte count %zu`'.", src_bytes, dest_bytes);
    }

    if(src_bytes != dest_bytes || memcmp(src_buf, dest_buf, src_bytes) != 0) {
        if(DCOPY_user_opts.reliable_filesystem) {
            LOG(DCOPY_LOG_ERR, "Compare mismatch when copying from file `%s'.", src_path);
            exit(EXIT_FAILURE);
        }
        else {
            LOG(DCOPY_LOG_ERR, "Compare mismatch when copying from file `%s'. Requeueing file.", src_path);
            newop = DCOPY_encode_operation(TREEWALK, 0, src_path, op->source_base_offset, op->dest_base_appendix, op->file_size);
            handle->enqueue(newop);
            free(newop);
        }
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
