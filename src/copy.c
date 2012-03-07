#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "copy.h"
#include "log.h"

/** Options specified by the user. */
extern DCOPY_options_t DCOPY_user_opts;

/** The loglevel that this instance of dcopy will output. */
extern DCOPY_loglevel  DCOPY_debug_level;

/** Statistics to gather for summary output. */
extern DCOPY_statistics_t DCOPY_statistics;

void DCOPY_do_copy(DCOPY_operation_t* op, CIRCLE_handle* handle)
{
    LOG(DCOPY_LOG_DBG, "Copy %s chunk %d", op->operand, op->chunk);

    char path[4096];
    sprintf(path, "%s/%s", DCOPY_user_opts.src_path[0], op->operand);
    FILE* in = fopen(path, "rb");

    if(!in) {
        LOG(DCOPY_LOG_ERR, "Unable to open %s", path);
        perror("open");
        return;
    }

    char newfile[4096];
    char buf[DCOPY_CHUNK_SIZE];
    //    void * buf = (void*) malloc(DCOPY_CHUNK_SIZE);
    sprintf(newfile, "%s/%s", DCOPY_user_opts.dest_path, op->operand);
    int outfd = open(newfile, O_RDWR | O_CREAT, 00644);

    if(!outfd) {
        LOG(DCOPY_LOG_ERR, "Unable to open %s", newfile);
        return;
    }

    if(fseek(in, DCOPY_CHUNK_SIZE * op->chunk, SEEK_SET) != 0) {
        LOG(DCOPY_LOG_ERR, "Couldn't seek %s", op->operand);
        perror("fseek");
        return;
    }

    size_t bytes = fread((void*)buf, 1, DCOPY_CHUNK_SIZE, in);

    if(bytes <= 0) {
        LOG(DCOPY_LOG_ERR, "Couldn't read %s", op->operand);
        perror("fread");
        return;
    }

    LOG(DCOPY_LOG_DBG, "Read %ld bytes.", bytes);

    lseek(outfd, DCOPY_CHUNK_SIZE * op->chunk, SEEK_SET);
    int qty = write(outfd, buf, bytes);

    if(qty > 0) {
        DCOPY_statistics.total_bytes_copied += qty;
    }

    LOG(DCOPY_LOG_DBG, "Wrote %ld bytes (%ld total).", bytes, DCOPY_statistics.total_bytes_copied);

    char* newop = DCOPY_encode_operation(CHECKSUM, op->chunk, op->operand);
    handle->enqueue(newop);

    free(newop);
    fclose(in);
    close(outfd);

    return;
}

/* EOF */
