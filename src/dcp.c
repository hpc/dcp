/*
 * A distributed copy tool.
 */

#include "log.h"
#include "dcp.h"

#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

DCOPY_loglevel  DCOPY_debug_level;

FILE* DCOPY_debug_stream;
int   CIRCLE_global_rank;

char* DEST_DIR;
char* TOP_DIR;
int   TOP_DIR_LEN;

char* DCOPY_op_string_table[] = {
    "COPY", "CHECKSUM", "STAT"
};

time_t DCOPY_time_started;
time_t DCOPY_time_finished;
size_t DCOPY_total_bytes_copied;

void (*DCOPY_jump_table[4])(DCOPY_operation_t* op, CIRCLE_handle* handle);

char* DCOPY_encode_operation(DCOPY_operation_code_t op, int chunk, char* operand)
{
    char* result = (char*) malloc(sizeof(char) * 4096);
    sprintf(result, "%d:%d:%s", chunk, op, operand);
    return result;
}

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

void DCOPY_process_dir(char* dir, CIRCLE_handle* handle)
{
    DIR* current_dir;
    char parent[2048];
    struct dirent* current_ent;
    char path[4096];
    int is_top_dir = !strcmp(dir, TOP_DIR);

    if(is_top_dir) {
        sprintf(path, "%s", dir);
    }
    else {
        sprintf(path, "%s/%s", TOP_DIR, dir);
    }

    current_dir = opendir(path);

    if(!current_dir) {
        LOG(DCOPY_LOG_ERR, "Unable to open dir: %s", path);
    }
    else {
        /* Read in each directory entry */
        while((current_ent = readdir(current_dir)) != NULL) {
            /* We don't care about . or .. */
            if((strncmp(current_ent->d_name, ".", 2)) && (strncmp(current_ent->d_name, "..", 3))) {
                LOG(DCOPY_LOG_DBG, "Dir entry %s / %s", dir, current_ent->d_name);

                if(is_top_dir) {
                    strcpy(parent, "");
                }
                else {
                    strcpy(parent, dir);
                }

                LOG(DCOPY_LOG_DBG, "Parent %s", parent);

                strcat(parent, "/");
                strcat(parent, current_ent->d_name);

                LOG(DCOPY_LOG_DBG, "Pushing [%s] <- [%s]", parent, dir);

                char* newop = DCOPY_encode_operation(STAT, 0, parent);
                handle->enqueue(newop);
                free(newop);
            }
        }
    }

    closedir(current_dir);
    return;
}

void DCOPY_do_stat(DCOPY_operation_t* op, CIRCLE_handle* handle)
{
    static struct stat st;
    static int status;
    int is_top_dir = !strcmp(op->operand, TOP_DIR);
    char path[4096];

    if(is_top_dir) {
        sprintf(path, "%s", TOP_DIR);
    }
    else {
        sprintf(path, "%s/%s", TOP_DIR, op->operand);
    }

    status = lstat(path, &st);

    if(status != EXIT_SUCCESS) {
        LOG(DCOPY_LOG_ERR, "Unable to stat \"%s\"", path);
        perror("stat");
    }
    else if(S_ISDIR(st.st_mode) && !(S_ISLNK(st.st_mode))) {
        char dir[2048];
        LOG(DCOPY_LOG_DBG, "Operand: %s Dir: %s", op->operand, DEST_DIR);

        if(is_top_dir) {
            sprintf(dir, "mkdir -p %s", op->operand);
        }
        else {
            sprintf(dir, "mkdir -p %s/%s", DEST_DIR, op->operand);
        }

        LOG(DCOPY_LOG_DBG, "Creating %s", dir);

        FILE* p = popen(dir, "r");
        pclose(p);
        DCOPY_process_dir(op->operand, handle);
    }
    else {
        int num_chunks = st.st_size / CHUNK_SIZE;
        LOG(DCOPY_LOG_DBG, "File size: %ld Chunks:%d Total: %d", st.st_size, num_chunks, num_chunks * CHUNK_SIZE);
        int i = 0;

        for(i = 0; i < num_chunks; i++) {
            char* newop = DCOPY_encode_operation(COPY, i, op->operand);
            handle->enqueue(newop);
            free(newop);
        }

        if(num_chunks * CHUNK_SIZE < st.st_size) {
            char* newop = DCOPY_encode_operation(COPY, i, op->operand);
            handle->enqueue(newop);
            free(newop);
        }
    }

    return;
}

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

DCOPY_operation_t* DCOPY_decode_operation(char* op)
{
    DCOPY_operation_t* ret = (DCOPY_operation_t*) malloc(sizeof(DCOPY_operation_t));

    ret->operand = (char*) malloc(sizeof(char) * 4096);
    ret->chunk = atoi(strtok(op, ":"));
    ret->code = atoi(strtok(NULL, ":"));
    ret->operand = strtok(NULL, ":");

    return ret;
}

void DCOPY_add_objects(CIRCLE_handle* handle)
{
    TOP_DIR_LEN = strlen(TOP_DIR);
    char* op = DCOPY_encode_operation(STAT, 0, TOP_DIR);

    handle->enqueue(op);

    free(op);
}

void DCOPY_process_objects(CIRCLE_handle* handle)
{
    char op[2048];

    /* Pop an item off the queue */
    LOG(DCOPY_LOG_DBG, "Popping, queue has %d elements", handle->local_queue_size());
    handle->dequeue(op);

    DCOPY_operation_t* opt = DCOPY_decode_operation(op);

    LOG(DCOPY_LOG_DBG, "Popped [%s]", opt->operand);
    LOG(DCOPY_LOG_DBG, "Operation: %d %s", opt->code, DCOPY_op_string_table[opt->code]);

    DCOPY_jump_table[opt->code](opt, handle);
    free(opt);

    return;
}

int main(int argc, char** argv)
{
    int c;

    DCOPY_jump_table[0] = DCOPY_do_copy;
    DCOPY_jump_table[1] = DCOPY_do_checksum;
    DCOPY_jump_table[2] = DCOPY_do_stat;
    DCOPY_total_bytes_copied = 0.0;

    DCOPY_debug_stream = stdout;
    DCOPY_debug_level = DCOPY_LOG_DBG;

    int CIRCLE_global_rank = CIRCLE_init(argc, argv, CIRCLE_DEFAULT_FLAGS);
    opterr = 0;

    TOP_DIR = DEST_DIR = NULL;

    while((c = getopt(argc, argv, "l:s:d:")) != -1) {
        switch(c) {
            case 's':
                TOP_DIR = optarg;
                break;
            case 'd':
                DEST_DIR = optarg;
                break;
            case 'l':
                DCOPY_debug_level = atoi(optarg);
                break;

            case '?':

                if(optopt == 'l') {
                    fprintf(stderr, "Option -%c requires an argument.\n", optopt);
                    exit(EXIT_FAILURE);
                }
                else if(isprint(optopt)) {
                    fprintf(stderr, "Unknown option `-%c'.\n", optopt);
                    exit(EXIT_FAILURE);
                }
                else {
                    fprintf(stderr,
                            "Unknown option character `\\x%x'.\n",
                            optopt);
                    exit(EXIT_FAILURE);
                }

            default:
                abort();
        }
    }

    if((TOP_DIR == NULL || DEST_DIR == NULL) && CIRCLE_global_rank == 0) {
        LOG(DCOPY_LOG_ERR, "You must specify a source and destination path.");
        exit(1);
    }

    time(&DCOPY_time_started);

    CIRCLE_cb_create(&DCOPY_add_objects);
    CIRCLE_cb_process(&DCOPY_process_objects);

    double start = CIRCLE_wtime();

    CIRCLE_begin();

    double end = CIRCLE_wtime() - start;

    CIRCLE_finalize();

    //double rate = DCOPY_total_bytes_copied / end;

    time(&DCOPY_time_finished);

    char starttime_str[256];
    char endtime_str[256];

    struct tm* localstart = localtime(&DCOPY_time_started);
    struct tm* localend = localtime(&DCOPY_time_finished);

    strftime(starttime_str, 256, "%b-%d-%Y,%H:%M:%S", localstart);
    strftime(endtime_str, 256, "%b-%d-%Y,%H:%M:%S", localend);

    LOG(DCOPY_LOG_INFO, "Filecopy run started at: %s", starttime_str);
    LOG(DCOPY_LOG_INFO, "Filecopy run completed at: %s", endtime_str);
    LOG(DCOPY_LOG_INFO, "Filecopy total time (seconds) for this run: %f", difftime(DCOPY_time_finished, DCOPY_time_started));
    LOG(DCOPY_LOG_INFO, "Transfer rate: %ld bytes in %lf seconds.", DCOPY_total_bytes_copied, end);

    exit(EXIT_SUCCESS);
}

/* EOF */
