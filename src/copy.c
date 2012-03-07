#include "copy.h"

/** The loglevel that this instance of dcopy will output. */
extern DCOPY_loglevel  DCOPY_debug_level;

void DCOPY_do_copy(DCOPY_operation_t* op, CIRCLE_handle* handle)
{
    LOG(DCOPY_LOG_DBG, "Copy %s chunk %d", op->operand, op->chunk);

    char path[4096];
    sprintf(path, "%s/%s", TOP_DIR, op->operand);
    FILE* in = fopen(path, "rb");

    if(!in) {
        LOG(DCOPY_LOG_ERR, "Unable to open %s", path);
        perror("open");
        return;
    }

    char newfile[4096];
    char buf[CHUNK_SIZE];
    //    void * buf = (void*) malloc(CHUNK_SIZE);
    sprintf(newfile, "%s/%s", DEST_DIR, op->operand);
    int outfd = open(newfile, O_RDWR | O_CREAT, 00644);

    if(!outfd) {
        LOG(DCOPY_LOG_ERR, "Unable to open %s", newfile);
        return;
    }

    if(fseek(in, CHUNK_SIZE * op->chunk, SEEK_SET) != 0) {
        LOG(DCOPY_LOG_ERR, "Couldn't seek %s", op->operand);
        perror("fseek");
        return;
    }

    size_t bytes = fread((void*)buf, 1, CHUNK_SIZE, in);

    if(bytes <= 0) {
        LOG(DCOPY_LOG_ERR, "Couldn't read %s", op->operand);
        perror("fread");
        return;
    }

    LOG(DCOPY_LOG_DBG, "Read %ld bytes.", bytes);

    lseek(outfd, CHUNK_SIZE * op->chunk, SEEK_SET);
    int qty = write(outfd, buf, bytes);

    if(qty > 0) {
        DCOPY_total_bytes_copied += qty;
    }

    LOG(DCOPY_LOG_DBG, "Wrote %ld bytes (%ld total).", bytes, DCOPY_total_bytes_copied);

    char* newop = DCOPY_encode_operation(CHECKSUM, op->chunk, op->operand);
    handle->enqueue(newop);

    free(newop);
    fclose(in);
    close(outfd);

    return;
}

/* EOF */
