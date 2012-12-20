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
    char dest_path[PATH_MAX];
    char buf[DCOPY_CHUNK_SIZE];
    char* source_path = op->operand;

    size_t bytes_read = 0;
    size_t bytes_written = 0;

    FILE* in;
    int outfd;

    if(op->dest_base_appendix == NULL) {
        sprintf(dest_path, "%s/%s", \
                DCOPY_user_opts.dest_path, \
                op->operand + op->source_base_offset + 1);
    }
    else {
        sprintf(dest_path, "%s/%s/%s", \
                DCOPY_user_opts.dest_path, \
                op->dest_base_appendix, \
                op->operand + op->source_base_offset + 1);
    }

    LOG(DCOPY_LOG_DBG, "Copying to destination path `%s' from source path `%s'.", dest_path, source_path);
    LOG(DCOPY_LOG_DBG, "Copying chunk number `%d' from source path `%s'.", op->chunk, source_path);

    in = fopen(source_path, "rb");

    if(!in) {
        LOG(DCOPY_LOG_ERR, "Unable to open source path `%s'. %s", \
            source_path, strerror(errno));
        return;
    }

    outfd = open(dest_path, O_RDWR | O_CREAT, 00644);

    if(outfd < 0) {
        /*
         * Since we might be trying a file to file copy, lets try to open
         * the base instead. If it really is a directory, we'll go ahead and
         * fail.
         */
        LOG(DCOPY_LOG_DBG, "Attempting to see if this is a file to file copy.");
        outfd = open(DCOPY_user_opts.dest_path, O_RDWR | O_CREAT, 00644);

        if(outfd < 0) {
            LOG(DCOPY_LOG_ERR, "Unable to open destination path `%s'. %s", \
                dest_path, strerror(errno));

            if(DCOPY_user_opts.reliable_filesystem) {
                LOG(DCOPY_LOG_ERR, "Retrying since unreliable filesystem was specified.");
                exit(EXIT_FAILURE);
            }

            return;
        }
    }

    if(fseek(in, DCOPY_CHUNK_SIZE * op->chunk, SEEK_SET) != 0) {
        LOG(DCOPY_LOG_ERR, "Couldn't seek in source path `%s'. %s", \
            source_path, strerror(errno));

        if(DCOPY_user_opts.reliable_filesystem) {
            LOG(DCOPY_LOG_ERR, "Retrying since unreliable filesystem was specified.");
            exit(EXIT_FAILURE);
        }

        return;
    }

    bytes_read = fread((void*)buf, 1, DCOPY_CHUNK_SIZE, in);

    if(bytes_read <= 0) {
        LOG(DCOPY_LOG_ERR, "Couldn't read from source path `%s'. %s", \
            source_path, strerror(errno));

        if(DCOPY_user_opts.reliable_filesystem) {
            LOG(DCOPY_LOG_ERR, "Retrying since unreliable filesystem was specified.");
            exit(EXIT_FAILURE);
        }

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
        char* newop = DCOPY_encode_operation(COMPARE, op->chunk, source_path, op->source_base_offset, op->dest_base_appendix);
        handle->enqueue(newop);
        free(newop);
    }

    fclose(in);
    close(outfd);

    return;
}

/* EOF */
