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
#include "treewalk.h"

/** Options specified by the user. */
extern DCOPY_options_t DCOPY_user_opts;

/** Statistics to gather for summary output. */
extern DCOPY_statistics_t DCOPY_statistics;

void DCOPY_do_copy(DCOPY_operation_t* op, CIRCLE_handle* handle)
{
    char dest_path[PATH_MAX];
    char buf[DCOPY_CHUNK_SIZE];
    char* source_path = op->operand;

    bool is_file_to_file_copy = false;
    bool unlink_on_failed_create = DCOPY_user_opts.force;

    size_t bytes_read = 0;
    size_t bytes_written = 0;

    FILE* in;
    int outfd;

    /* If we need to stat before an unlink. */
    struct stat sb;

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

    in = fopen(source_path, "rb");

    if(!in) {
        LOG(DCOPY_LOG_ERR, "Unable to open source path `%s'. %s", \
            source_path, strerror(errno));

        if(DCOPY_user_opts.reliable_filesystem) {
            exit(EXIT_FAILURE);
        }

        LOG(DCOPY_LOG_ERR, "Retrying since unreliable filesystem was specified.");
        return;
    }

    outfd = open(dest_path, O_RDWR | O_CREAT, 00644);

    /* Force option handing for recursive-style copies. */
    if((outfd < 0) && unlink_on_failed_create) {

        /* If the stat() was successful and we're not a directory, lets try an unlink. */
        if((stat(dest_path, &sb) == 0)) {
            if(!S_ISDIR(sb.st_mode)) {
                LOG(DCOPY_LOG_DBG, "Destination file creation failed on a recursive-style copy.");
                LOG(DCOPY_LOG_DBG, "Attempting to unlink since force is enabled.");

                if(unlink(dest_path) != 0) {
                    LOG(DCOPY_LOG_ERR, "Could not unlink destination. %s", strerror(errno));

                    if(DCOPY_user_opts.reliable_filesystem) {
                        exit(EXIT_FAILURE);
                    }

                    LOG(DCOPY_LOG_ERR, "Retrying since unreliable filesystem was specified.");
                    return;
                }

                /* Try it again. */
                outfd = open(dest_path, O_RDWR | O_CREAT, 00644);

                if(outfd < 0) {
                    LOG(DCOPY_LOG_ERR, "Could not open destination after an unlink. %s", strerror(errno));

                    if(DCOPY_user_opts.reliable_filesystem) {
                        exit(EXIT_FAILURE);
                    }

                    LOG(DCOPY_LOG_ERR, "Retrying since unreliable filesystem was specified.");
                    return;
                }
            }
        }
    }

    /* Fallback to file-to-file copy since it doesn't look like we're recursive.. */
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

            /* Force option handing for file-to-file style copies. */
            if(unlink_on_failed_create) {

                /* If the stat() was successful and we're not a directory, lets try an unlink. */
                if((stat(DCOPY_user_opts.dest_path, &sb) == 0)) {
                    if(!S_ISDIR(sb.st_mode)) {
                        LOG(DCOPY_LOG_DBG, "Destination file creation failed on a file-to-file style copy.");
                        LOG(DCOPY_LOG_DBG, "Attempting to unlink since force is enabled.");

                        if(unlink(DCOPY_user_opts.dest_path) != 0) {
                            LOG(DCOPY_LOG_ERR, "Could not unlink destination. %s", strerror(errno));

                            if(DCOPY_user_opts.reliable_filesystem) {
                                exit(EXIT_FAILURE);
                            }

                            LOG(DCOPY_LOG_ERR, "Retrying since unreliable filesystem was specified.");
                            return;
                        }

                        /* Try it again. */
                        outfd = open(DCOPY_user_opts.dest_path, O_RDWR | O_CREAT, 00644);

                        if(outfd < 0) {
                            LOG(DCOPY_LOG_ERR, "Could not open destination after an unlink. %s", strerror(errno));

                            if(DCOPY_user_opts.reliable_filesystem) {
                                exit(EXIT_FAILURE);
                            }

                            LOG(DCOPY_LOG_ERR, "Retrying since unreliable filesystem was specified.");
                            return;
                        }
                    }
                }
            }
            else {
                if(DCOPY_user_opts.reliable_filesystem) {
                    exit(EXIT_FAILURE);
                }

                LOG(DCOPY_LOG_ERR, "Retrying since unreliable filesystem was specified.");
                return;
            }
        }
        else {
            is_file_to_file_copy = true;
        }
    }

    if(is_file_to_file_copy) {
        LOG(DCOPY_LOG_INFO, "Copying to destination path `%s' from source path `%s' (chunk number %d).", \
            DCOPY_user_opts.dest_path, source_path, op->chunk);
        /*
                stat(DCOPY_user_opts.dest_path, &sb);
                LOG(DCOPY_LOG_DBG, "Destination file size is `%zu'.", sb.st_size);
        */
    }
    else {
        LOG(DCOPY_LOG_INFO, "Copying to destination path `%s' from source path `%s' (chunk number %d).", \
            dest_path, source_path, op->chunk);

        /*
                stat(dest_path, &sb);
                LOG(DCOPY_LOG_DBG, "Destination file size is `%zu'.", sb.st_size);
        */
    }

    if(fseek(in, DCOPY_CHUNK_SIZE * op->chunk, SEEK_SET) != 0) {
        LOG(DCOPY_LOG_ERR, "Couldn't seek in source path `%s'. %s", \
            source_path, strerror(errno));

        if(DCOPY_user_opts.reliable_filesystem) {
            exit(EXIT_FAILURE);
        }

        LOG(DCOPY_LOG_ERR, "Retrying since unreliable filesystem was specified.");
        return;
    }

    bytes_read = fread((void*)buf, 1, DCOPY_CHUNK_SIZE, in);

    if(bytes_read <= 0) {
        LOG(DCOPY_LOG_ERR, "Couldn't read from source path `%s'. %s", \
            source_path, strerror(errno));

        if(DCOPY_user_opts.reliable_filesystem) {
            exit(EXIT_FAILURE);
        }

        LOG(DCOPY_LOG_ERR, "Retrying since unreliable filesystem was specified.");
        return;
    }

    LOG(DCOPY_LOG_DBG, "Writing `%zu' bytes at offset `%d'.", bytes_read, DCOPY_CHUNK_SIZE * op->chunk);

    lseek(outfd, DCOPY_CHUNK_SIZE * op->chunk, SEEK_SET);
    bytes_written = write(outfd, buf, bytes_read);

    if(bytes_written > 0) {
        DCOPY_statistics.total_bytes_copied += bytes_written;
    }

    LOG(DCOPY_LOG_DBG, "Wrote %zu bytes (%ld total).", bytes_written, DCOPY_statistics.total_bytes_copied);

    char* newop = DCOPY_encode_operation(CLEANUP, op->chunk, source_path, op->source_base_offset, op->dest_base_appendix, op->file_size);
    handle->enqueue(newop);
    free(newop);

    if(fclose(in) < 0) {
        LOG(DCOPY_LOG_DBG, "Close on source file failed. %s", strerror(errno));
    }

    if(close(outfd) < 0) {
        LOG(DCOPY_LOG_DBG, "Close on destination file failed. %s", strerror(errno));
    }

    return;
}

/* EOF */
