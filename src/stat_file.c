/* See the file "COPYING" for the full license governing this code. */

#include <dirent.h>
#include <errno.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "dcp.h"
#include "stat_file.h"
#include "log.h"

/** Options specified by the user. */
extern DCOPY_options_t DCOPY_user_opts;

/** The loglevel that this instance of dcopy will output. */
extern DCOPY_loglevel DCOPY_debug_level;

/**
 * Determine if the specified path is a directory.
 */
bool DCOPY_is_directory(char* path)
{
    struct stat statbuf;
    int s = lstat(path, &statbuf);

    if(s == -1) {
        return false;
    }

    return (S_ISDIR(statbuf.st_mode) && !(S_ISLNK(statbuf.st_mode)));
}

/**
 * Determine if the specified path is a regular file.
 */
bool DCOPY_is_regular_file(char* path)
{
    struct stat statbuf;
    int s = lstat(path, &statbuf);

    if(s == -1) {
        return false;
    }

    return (S_ISREG(statbuf.st_mode) && !(S_ISLNK(statbuf.st_mode)));
}

void DCOPY_do_stat(DCOPY_operation_t* op, CIRCLE_handle* handle)
{
    struct stat statbuf;
    int s = lstat(op->operand, &statbuf);

    LOG(DCOPY_LOG_DBG, "Performing a stat Operation.");
    LOG(DCOPY_LOG_DBG, "Source object is `%s'.", op->operand);

    if(s < 0) {
        LOG(DCOPY_LOG_DBG, "Could not stat file at `%s'.", op->operand);
        exit(EXIT_FAILURE);
    }

    if(S_ISDIR(statbuf.st_mode) && !(S_ISLNK(statbuf.st_mode))) {
        LOG(DCOPY_LOG_DBG, "Stat operation found a directory at `%s'.", op->operand);
        DCOPY_stat_process_dir(op->operand, handle);
    }
    else if(S_ISREG(statbuf.st_mode) && !(S_ISLNK(statbuf.st_mode))) {
        LOG(DCOPY_LOG_DBG, "Stat operation found a file at `%s'.", op->operand);
        DCOPY_stat_process_file(op->operand, statbuf.st_size, handle);
    }
    else {
        LOG(DCOPY_LOG_DBG, "Encounted an unsupported file type at `%s'.", op->operand);
        exit(EXIT_FAILURE);
    }
}

void DCOPY_stat_process_file(char* path, size_t file_size, CIRCLE_handle* handle)
{
    size_t chunk_index;
    size_t num_chunks = file_size / DCOPY_CHUNK_SIZE;

    LOG(DCOPY_LOG_DBG, "File size is `%ld' with chunks `%zu' (total `%zu').", \
        file_size, num_chunks, num_chunks * DCOPY_CHUNK_SIZE);

    /* Encode and nqueue each chunk of the file for processing later. */
    for(chunk_index = 0; chunk_index < num_chunks; chunk_index++) {
        char* newop = DCOPY_encode_operation(COPY, chunk_index, path);
        handle->enqueue(newop);
        free(newop);
    }

    /* Encode and enqueue the last partial chunk. */
    if(num_chunks * DCOPY_CHUNK_SIZE < file_size) {
        char* newop = DCOPY_encode_operation(COPY, chunk_index, path);
        handle->enqueue(newop);
        free(newop);
    }
}

void DCOPY_stat_process_dir(char* path, CIRCLE_handle* handle)
{
    DIR* curr_dir;
    char* curr_dir_name;

    struct dirent* curr_ent;
    char cmd_buf[PATH_MAX];
    char newop_path[PATH_MAX];

    sprintf(cmd_buf, "mkdir -p %s/%s", \
            DCOPY_user_opts.dest_path, \
            path);
    LOG(DCOPY_LOG_DBG, "Creating directory with cmd `%s'.", cmd_buf);

    FILE* p = popen(cmd_buf, "r");
    pclose(p);

    curr_dir = opendir(path);

    if(curr_dir == NULL) {
        LOG(DCOPY_LOG_ERR, "Unable to open dir `%s'. %s", \
            path, strerror(errno));
        exit(EXIT_FAILURE);
    }
    else {
        while((curr_ent = readdir(curr_dir)) != NULL) {
            curr_dir_name = curr_ent->d_name;

            /* We don't care about . or .. */
            if((strncmp(curr_dir_name, ".", 2)) && (strncmp(curr_dir_name, "..", 3))) {

                LOG(DCOPY_LOG_DBG, "Stat enqueue dir `%s' with base `%s'.", \
                    curr_dir_name, path);

                sprintf(newop_path, "%s/%s", path, curr_dir_name);

                char* newop = DCOPY_encode_operation(STAT, 0, newop_path);
                handle->enqueue(newop);

                free(newop);
            }
        }
    }

    closedir(curr_dir);
    return;
}

/* EOF */
