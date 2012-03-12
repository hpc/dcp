/* See the file "COPYING" for the full license governing this code. */

#include <dirent.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "dcp.h"
#include "filestat.h"
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
    stat(path, &statbuf);

    return S_ISDIR(statbuf.st_mode);
}

/**
 * Determine if the specified path is a regular file.
 */
bool DCOPY_is_regular_file(char* path)
{
    struct stat statbuf;
    stat(path, &statbuf);

    return S_ISREG(statbuf.st_mode);
}

void DCOPY_do_stat(DCOPY_operation_t* op, CIRCLE_handle* handle)
{
    static struct stat st;
    static int status;
    char path[PATH_MAX];
    char dir[2048];

    LOG(DCOPY_LOG_DBG, "STAT - operand is `%s', subop is `%s', baseidx is `%d', dest is `%s'.", \
        op->operand, op->operand + op->base_index, op->base_index, DCOPY_user_opts.dest_path);

    status = lstat(op->operand, &st);

    if(status != EXIT_SUCCESS) {
        LOG(DCOPY_LOG_ERR, "Unable to stat \"%s\"", op->operand);
        perror("stat");
    }
    else if(S_ISDIR(st.st_mode) && !(S_ISLNK(st.st_mode))) {
        LOG(DCOPY_LOG_DBG, "Operand: %s Dir: %s", op->operand, DCOPY_user_opts.dest_path);

        strncpy(path, op->operand + op->base_index, PATH_MAX);
        sprintf(dir, "mkdir -p %s/%s", DCOPY_user_opts.dest_path, path);
        LOG(DCOPY_LOG_DBG, "Creating %s", dir);

        FILE* p = popen(dir, "r");
        pclose(p);

        DCOPY_process_dir(op->operand, handle, op->base_index);
    }
    else {
        int num_chunks = st.st_size / DCOPY_CHUNK_SIZE;
        int i = 0;

        LOG(DCOPY_LOG_DBG, "File size: %ld Chunks:%d Total: %d", st.st_size, num_chunks, num_chunks * DCOPY_CHUNK_SIZE);

        for(i = 0; i < num_chunks; i++) {
            char* newop = DCOPY_encode_operation(COPY, i, op->operand, op->base_index);
            handle->enqueue(newop);
            free(newop);
        }

        if(num_chunks * DCOPY_CHUNK_SIZE < st.st_size) {
            char* newop = DCOPY_encode_operation(COPY, i, op->operand, op->base_index);
            handle->enqueue(newop);
            free(newop);
        }
    }

    return;
}

void DCOPY_process_dir(char* dir, CIRCLE_handle* handle, uint16_t base_index)
{
    DIR* current_dir;
    struct dirent* current_ent;
    char path[PATH_MAX];

    current_dir = opendir(dir);

    if(!current_dir) {
        LOG(DCOPY_LOG_ERR, "Unable to open dir: %s", path);
    }
    else {
        /* Read in each directory entry */
        while((current_ent = readdir(current_dir)) != NULL) {
            /* We don't care about . or .. */
            if((strncmp(current_ent->d_name, ".", 2)) && (strncmp(current_ent->d_name, "..", 3))) {
                LOG(DCOPY_LOG_DBG, "Stat for %s/%s", dir, current_ent->d_name);

                sprintf(path, "%s/%s", dir, current_ent->d_name);

                char* newop = DCOPY_encode_operation(STAT, 0, path, base_index);
                handle->enqueue(newop);
                free(newop);
            }
        }
    }

    closedir(current_dir);
    return;
}

/* EOF */
