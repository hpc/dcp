#include "log.h"
#include "checksum.h"

/** The loglevel that this instance of dcopy will output. */
extern DCOPY_loglevel  DCOPY_debug_level;

void DCOPY_do_checksum(DCOPY_operation_t* op, CIRCLE_handle* handle)
{
    LOG(DCOPY_LOG_DBG, "Checksum %s chunk %d", op->operand, op->chunk);
    char path[4096];
    sprintf(path, "%s/%s", TOP_DIR, op->operand);
    FILE* old = fopen(path, "rb");

    if(!old) {
        LOG(DCOPY_LOG_ERR, "Unable to open file %s", path);
        return;
    }

    char newfile[4096];
    void* newbuf = (void*) malloc(CHUNK_SIZE);
    void* oldbuf = (void*) malloc(CHUNK_SIZE);
    sprintf(newfile, "%s/%s", DEST_DIR, op->operand);
    FILE* new = fopen(newfile, "rb");

    if(!new) {
        LOG(DCOPY_LOG_ERR, "Unable to open file %s", newfile);
        perror("checksum open");
        char* newop = DCOPY_encode_operation(CHECKSUM, op->chunk, op->operand);
        handle->enqueue(newop);
        free(newop);
        return;
    }

    fseek(new, CHUNK_SIZE * op->chunk, SEEK_SET);
    fseek(old, CHUNK_SIZE * op->chunk, SEEK_SET);
    size_t newbytes = fread(newbuf, 1, CHUNK_SIZE, new);
    size_t oldbytes = fread(oldbuf, 1, CHUNK_SIZE, old);

    if(newbytes != oldbytes || memcmp(newbuf, oldbuf, newbytes) != 0) {
        LOG(DCOPY_LOG_ERR, "Incorrect checksum, requeueing file (%s).", op->operand);
        char* newop = DCOPY_encode_operation(STAT, 0, op->operand);
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
