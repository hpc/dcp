/* See the file "COPYING" for the full license governing this code. */

#include <libgen.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "checksum.h"

/** Options specified by the user. */
extern DCOPY_options_t DCOPY_user_opts;

/** The loglevel that this instance of dcopy will output. */
extern DCOPY_loglevel  DCOPY_debug_level;

void DCOPY_do_checksum(DCOPY_operation_t* op, CIRCLE_handle* handle)
{
    FILE* old;
    FILE* new;

    size_t newbytes;
    size_t oldbytes;

    char* newop;
    char* base_operand;

    char newfile[PATH_MAX];
    char tmppath[PATH_MAX];

    void* newbuf = (void*) malloc(DCOPY_CHUNK_SIZE);
    void* oldbuf = (void*) malloc(DCOPY_CHUNK_SIZE);

    /** If we have a file, grab the basename and append. */
    if(strlen(op->operand + op->base_index) < 1) {
        strncpy(tmppath, op->operand, PATH_MAX);
        base_operand = basename(tmppath);

        sprintf(newfile, "%s%s/%s", DCOPY_user_opts.dest_path, op->operand + op->base_index, base_operand);
    }
    else {
        sprintf(newfile, "%s%s", DCOPY_user_opts.dest_path, op->operand + op->base_index);
    }

    LOG(DCOPY_LOG_DBG, "Comparing (chunk %d) original `%s' against `%s'", \
        op->chunk, op->operand, newfile);

    old = fopen(op->operand, "rb");

    if(!old) {
        LOG(DCOPY_LOG_ERR, "Unable to open old file %s", op->operand);
        return;
    }

    new = fopen(newfile, "rb");

    if(!new) {
        LOG(DCOPY_LOG_ERR, "Unable to open new file %s", newfile);
        perror("checksum open");

        newop = DCOPY_encode_operation(CHECKSUM, op->chunk, op->operand, op->base_index);
        handle->enqueue(newop);
        free(newop);

        return;
    }

    fseek(new, DCOPY_CHUNK_SIZE * op->chunk, SEEK_SET);
    fseek(old, DCOPY_CHUNK_SIZE * op->chunk, SEEK_SET);

    newbytes = fread(newbuf, 1, DCOPY_CHUNK_SIZE, new);
    oldbytes = fread(oldbuf, 1, DCOPY_CHUNK_SIZE, old);

    if(newbytes != oldbytes || memcmp(newbuf, oldbuf, newbytes) != 0) {
        LOG(DCOPY_LOG_ERR, "Incorrect checksum, requeueing file (%s).", op->operand);

        newop = DCOPY_encode_operation(STAT, 0, op->operand, op->base_index);
        handle->enqueue(newop);
        free(newop);
    }
    else {
        LOG(DCOPY_LOG_DBG, "File (%s) chunk %d OK.", newfile, op->chunk);
    }

    fclose(new);
    fclose(old);

    free(newbuf);
    free(oldbuf);

    return;
}

/* EOF */
