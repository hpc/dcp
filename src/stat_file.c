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

    if(s < 0) {
        LOG(DCOPY_LOG_DBG, "Could not stat file at `%s'.", op->operand);
        exit(EXIT_FAILURE);
    }

    if(S_ISDIR(statbuf.st_mode) && !(S_ISLNK(statbuf.st_mode))) {
        LOG(DCOPY_LOG_DBG, "Stat operation found a directory at `%s'.", op->operand);
        DCOPY_stat_process_dir(op, handle);
    }
    else if(S_ISREG(statbuf.st_mode) && !(S_ISLNK(statbuf.st_mode))) {
        LOG(DCOPY_LOG_DBG, "Stat operation found a file at `%s'.", op->operand);
        DCOPY_stat_process_file(op, statbuf.st_size, handle);
    }
    else {
        LOG(DCOPY_LOG_DBG, "Encounted an unsupported file type at `%s'.", op->operand);
        exit(EXIT_FAILURE);
    }
}

/**
 * This function inputs a file and creates chunk operations that get placed
 * onto the libcircle queue for future processing by the copy stage.
 */
void DCOPY_stat_process_file(DCOPY_operation_t* op, size_t file_size, CIRCLE_handle* handle)
{
    size_t chunk_index;

    /* Round up. FIXME: this generates some really ugly opcodes. */
    size_t num_chunks = (size_t)(((double)file_size) / ((double)DCOPY_CHUNK_SIZE + 0.5f));

    LOG(DCOPY_LOG_DBG, "File `%s' size is `%ld' with chunks `%zu' (total `%zu').", \
        op->operand, file_size, num_chunks, num_chunks * DCOPY_CHUNK_SIZE);

    /* Encode and nqueue each chunk of the file for processing later. */
    for(chunk_index = 0; chunk_index < num_chunks; chunk_index++) {
        char* newop = DCOPY_encode_operation(COPY, chunk_index, op->operand, op->source_base_offset, op->dest_base_appendix);
        handle->enqueue(newop);
        free(newop);
    }

    /* Encode and enqueue the last partial chunk. */
    if(num_chunks * DCOPY_CHUNK_SIZE < file_size) {
        char* newop = DCOPY_encode_operation(COPY, chunk_index, op->operand, op->source_base_offset, op->dest_base_appendix);
        handle->enqueue(newop);
        free(newop);
    }
}

/**
 * This function reads the contents of a directory and generates appropriate
 * libcircle operations for every object in the directory. It then places those
 * operations on the libcircle queue and returns.
 */
void DCOPY_stat_process_dir(DCOPY_operation_t* op, CIRCLE_handle* handle)
{
    DIR* curr_dir;
    char* curr_dir_name;

    struct dirent* curr_ent;
    char cmd_buf[PATH_MAX];
    char newop_path[PATH_MAX];

    LOG(DCOPY_LOG_DBG, "Using dest base appendix of `%d'.", *(op->dest_base_appendix));

    if(op->dest_base_appendix != NULL) {
        sprintf(cmd_buf, "mkdir -p %s/%s/%s", \
                DCOPY_user_opts.dest_path, \
                op->dest_base_appendix, \
                op->operand + op->source_base_offset);
    }
    else {
        sprintf(cmd_buf, "mkdir -p %s/%s", \
                DCOPY_user_opts.dest_path, \
                op->operand + op->source_base_offset);
    }

    LOG(DCOPY_LOG_DBG, "Creating directory with command `%s'.", cmd_buf);

    FILE* p = popen(cmd_buf, "r");
    pclose(p);

    curr_dir = opendir(op->operand);

    if(curr_dir == NULL) {
        LOG(DCOPY_LOG_ERR, "Unable to open dir `%s'. %s", \
            op->operand, strerror(errno));
        exit(EXIT_FAILURE);
    }
    else {
        while((curr_ent = readdir(curr_dir)) != NULL) {
            curr_dir_name = curr_ent->d_name;

            /* We don't care about . or .. */
            if((strncmp(curr_dir_name, ".", 2)) && (strncmp(curr_dir_name, "..", 3))) {

                LOG(DCOPY_LOG_DBG, "Stat operation is enqueueing directory `%s' using base `%s'.", \
                    op->operand, op->operand + op->source_base_offset);

                sprintf(newop_path, "%s/%s", op->operand, curr_dir_name);

                char* newop = DCOPY_encode_operation(STAT, 0, newop_path, op->source_base_offset, op->dest_base_appendix);
                handle->enqueue(newop);

                free(newop);
            }
        }
    }

    closedir(curr_dir);
    return;
}

/* EOF */
