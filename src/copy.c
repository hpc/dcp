/* See the file "COPYING" for the full license governing this code. */

#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "copy.h"
#include "log.h"
#include "filestat.h"

/** Options specified by the user. */
extern DCOPY_options_t DCOPY_user_opts;

/** The loglevel that this instance of dcopy will output. */
extern DCOPY_loglevel  DCOPY_debug_level;

/** Statistics to gather for summary output. */
extern DCOPY_statistics_t DCOPY_statistics;

void DCOPY_do_copy(DCOPY_operation_t* op, CIRCLE_handle* handle)
{
    char new_file_path[PATH_MAX];
    char buf[DCOPY_CHUNK_SIZE];

    size_t bytes_read = 0;
    size_t bytes_written = 0;

    FILE* in;
    int outfd;

    LOG(DCOPY_LOG_DBG, "Copy, operand is `%s', subop is `%s', baseidx is `%d', dest is `%s'.", \
        op->operand, op->operand + op->base_index, op->base_index, DCOPY_user_opts.dest_path);
    LOG(DCOPY_LOG_DBG, "Copy, `%s' chunk `%d'.", op->operand, op->chunk);

    sprintf(new_file_path, "%s%s", DCOPY_user_opts.dest_path, op->operand + op->base_index);

    in = fopen(op->operand, "rb");
    outfd = open(new_file_path, O_RDWR | O_CREAT, 00644);

    if(!in) {
        LOG(DCOPY_LOG_ERR, "Unable to open `%s'. %s", \
            op->operand, strerror(errno));
        return;
    }

    if(!outfd) {
        LOG(DCOPY_LOG_ERR, "Unable to open `%s'. %s", \
            new_file_path, strerror(errno));
        return;
    }

    if(fseek(in, DCOPY_CHUNK_SIZE * op->chunk, SEEK_SET) != 0) {
        LOG(DCOPY_LOG_ERR, "Couldn't seek `%s'. %s", \
            op->operand, strerror(errno));
        return;
    }

    bytes_read = fread((void*)buf, 1, DCOPY_CHUNK_SIZE, in);

    if(bytes_read <= 0) {
        LOG(DCOPY_LOG_ERR, "Couldn't read `%s'. %s", \
            op->operand, strerror(errno));
        return;
    }

    LOG(DCOPY_LOG_DBG, "Copy operation, we read `%zu' bytes.", bytes_read);

    lseek(outfd, DCOPY_CHUNK_SIZE * op->chunk, SEEK_SET);
    bytes_written = write(outfd, buf, bytes_read);

    if(bytes_written > 0) {
        DCOPY_statistics.total_bytes_copied += bytes_written;
    }

    LOG(DCOPY_LOG_DBG, "Wrote %zu bytes (%ld total).", bytes_written, DCOPY_statistics.total_bytes_copied);

    /*
     * If the user is feeling brave, this is where we let them skip the
     * comparison stage.
     */
    if(!DCOPY_user_opts.skip_compare) {
        char* newop = DCOPY_encode_operation(CHECKSUM, op->chunk, op->operand, op->base_index);
        handle->enqueue(newop);
        free(newop);
    }

    fclose(in);
    close(outfd);

    return;
}

/* EOF */
