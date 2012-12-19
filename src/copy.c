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
#include "stat_file.h"

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
    char file_file_buf[PATH_MAX];

    size_t bytes_read = 0;
    size_t bytes_written = 0;

    FILE* in;
    int outfd;

    LOG(DCOPY_LOG_DBG, "Copying from source object `%s' to destination object `%s'.", \
        op->operand, DCOPY_user_opts.dest_path);
    LOG(DCOPY_LOG_DBG, "Copying source object `%s' chunk number `%d'.", op->operand, op->chunk);

    sprintf(new_file_path, "%s%s", \
            DCOPY_user_opts.dest_path, \
            op->operand);
    LOG(DCOPY_LOG_DBG, "Copying to destination path `%s'.", new_file_path);

    in = fopen(op->operand, "rb");

    if(!in) {
        LOG(DCOPY_LOG_ERR, "Unable to open original `%s'. %s", \
            op->operand, strerror(errno));
        return;
    }

    outfd = open(new_file_path, O_RDWR | O_CREAT, 00644);

    if(outfd < 0) {
        /*
         * Since we might be trying a file to file copy, lets try to open
         * the base instead. If it really is a directory, we'll go ahead and
         * fail.
         */
        strncpy(file_file_buf, op->operand, PATH_MAX);
        file_file_buf[0] = '\0';

        sprintf(file_file_buf, "%s%s", file_file_buf, \
                DCOPY_user_opts.dest_path + DCOPY_user_opts.dest_base_index);

        LOG(DCOPY_LOG_DBG, "Attempting to open parent of new file `%s'.", file_file_buf);

        outfd = open(file_file_buf, O_RDWR | O_CREAT, 00644);

        if(outfd < 0) {
            LOG(DCOPY_LOG_ERR, "Unable to open new `%s'. %s", \
                new_file_path, strerror(errno));
            return;
        }
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
        char* newop = DCOPY_encode_operation(COMPARE, op->chunk, op->operand);
        handle->enqueue(newop);
        free(newop);
    }

    fclose(in);
    close(outfd);

    return;
}

/* EOF */
