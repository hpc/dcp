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
    FILE* old;
    FILE* new;

    size_t newbytes;
    size_t oldbytes;

    char* newop;

    char newfile[PATH_MAX];

    void* newbuf = (void*) malloc(DCOPY_CHUNK_SIZE);
    void* oldbuf = (void*) malloc(DCOPY_CHUNK_SIZE);

    LOG(DCOPY_LOG_DBG, "Comparing source `%s' object to destination object `%s'.", \
        op->operand, DCOPY_user_opts.dest_path);

    LOG(DCOPY_LOG_DBG, "Comparing source object `%s' (chunk number `%d') against destination `%s'.", \
        op->operand, op->chunk, newfile);

    old = fopen(op->operand, "rb");
    new = fopen(newfile, "rb");

    if(!old) {
        LOG(DCOPY_LOG_ERR, "Compare stage is unable to open old file `%s'. %s", \
            op->operand, strerror(errno));
        return;
    }

    if(!new) {
        LOG(DCOPY_LOG_ERR, "Compare stage is unable to open new file `%s'. %s", \
            newfile, strerror(errno));

        newop = DCOPY_encode_operation(COMPARE, op->chunk, op->operand);
        handle->enqueue(newop);
        free(newop);

        return;
    }

    fseek(new, DCOPY_CHUNK_SIZE * op->chunk, SEEK_SET);
    fseek(old, DCOPY_CHUNK_SIZE * op->chunk, SEEK_SET);

    newbytes = fread(newbuf, 1, DCOPY_CHUNK_SIZE, new);
    oldbytes = fread(oldbuf, 1, DCOPY_CHUNK_SIZE, old);

    if(newbytes != oldbytes || memcmp(newbuf, oldbuf, newbytes) != 0) {
        LOG(DCOPY_LOG_ERR, "Compare mismatch! Requeueing file `%s'.", op->operand);

        newop = DCOPY_encode_operation(STAT, 0, op->operand);
        handle->enqueue(newop);
        free(newop);
    }
    else {
        LOG(DCOPY_LOG_DBG, "File `%s' chunk `%d' compare ok.", newfile, op->chunk);
    }

    fclose(new);
    fclose(old);

    free(newbuf);
    free(oldbuf);

    return;
}

/* EOF */
