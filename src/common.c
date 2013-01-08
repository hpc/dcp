/* See the file "COPYING" for the full license governing this code. */

#include "common.h"
#include "handle_args.h"

#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/** The loglevel that this instance of dcopy will output. */
DCOPY_loglevel DCOPY_debug_level;

/** Where we should keep statistics related to this file copy. */
DCOPY_statistics_t DCOPY_statistics;

/** Where we should store options specified by the user. */
DCOPY_options_t DCOPY_user_opts;

/** Where debug output should go. */
FILE* DCOPY_debug_stream;

/** What rank the current process is. */
int CIRCLE_global_rank;

/** A table of function pointers used for core operation. */
void (*DCOPY_jump_table[5])(DCOPY_operation_t* op, CIRCLE_handle* handle);

void DCOPY_retry_failed_operation(DCOPY_operation_code_t target, \
                                  CIRCLE_handle* handle, \
                                  DCOPY_operation_t* op)
{
    char* new_op;

    if(DCOPY_user_opts.reliable_filesystem) {
        LOG(DCOPY_LOG_ERR, "Not retrying failed operation. " \
            "Reliable filesystem is specified.");
        exit(EXIT_FAILURE);
    }
    else {
        LOG(DCOPY_LOG_INFO, "Attempting to retry operation.");

        new_op = DCOPY_encode_operation(target, op->chunk, op->operand, \
                                        op->source_base_offset, \
                                        op->dest_base_appendix, op->file_size);

        handle->enqueue(new_op);
        free(new_op);
    }

    return;
}

/**
 * Encode an operation code for use on the distributed queue structure.
 */
char* DCOPY_encode_operation(DCOPY_operation_code_t code, \
                             uint32_t chunk, \
                             char* operand, \
                             uint16_t source_base_offset, \
                             char* dest_base_appendix, \
                             uint64_t file_size)
{
    char* op = (char*) malloc(sizeof(char) * CIRCLE_MAX_STRING_LEN);
    int op_size = 0;

    op_size += sprintf(op, "%" PRIu64 ":%" PRIu32 ":%" PRIu16 ":%d:%s", \
                       file_size, chunk, source_base_offset, code, operand);

    if(dest_base_appendix) {
        op_size += sprintf(op + op_size, ":%s", dest_base_appendix);
    }

    /*
     * FIXME: This requires architecture changes in libcircle -- a redesign of
     * internal queue data structures to allow void* types as queue elements
     * instead of null terminated strings. Ignoring this problem by commenting
     * out this check will likely cause silent data corruption.
     */
    if((op_size + 1) > CIRCLE_MAX_STRING_LEN) {
        LOG(DCOPY_LOG_DBG, \
            "Exceeded libcircle message size due to large file path. " \
            "This is a known bug in dcp that we intend to fix. Sorry!");
        exit(EXIT_FAILURE);
    }

    return op;
}

/**
 * Decode the operation code from a message on the distributed queue structure.
 */
DCOPY_operation_t* DCOPY_decode_operation(char* op)
{
    DCOPY_operation_t* ret = (DCOPY_operation_t*) malloc(sizeof(DCOPY_operation_t));

    if(sscanf(strtok(op, ":"), "%" SCNu64, &(ret->file_size)) != 1) {
        LOG(DCOPY_LOG_ERR, "Could not decode file size attribute.");
        exit(EXIT_FAILURE);
    }

    if(sscanf(strtok(NULL, ":"), "%" SCNu32, &(ret->chunk)) != 1) {
        LOG(DCOPY_LOG_ERR, "Could not decode chunk index attribute.");
        exit(EXIT_FAILURE);
    }

    if(sscanf(strtok(NULL, ":"), "%" SCNu16, &(ret->source_base_offset)) != 1) {
        LOG(DCOPY_LOG_ERR, "Could not decode source base offset attribute.");
        exit(EXIT_FAILURE);
    }

    if(sscanf(strtok(NULL, ":"), "%d", (int*) & (ret->code)) != 1) {
        LOG(DCOPY_LOG_ERR, "Could not decode stage code attribute.");
        exit(EXIT_FAILURE);
    }

    ret->operand            = strtok(NULL, ":");
    ret->dest_base_appendix = strtok(NULL, ":");

    return ret;
}

/**
 * The initial seeding callback for items to process on the distributed queue
 * structure. We all all of our source items to the queue here.
 */
void DCOPY_add_objects(CIRCLE_handle* handle)
{
    DCOPY_enqueue_work_objects(handle);
}

/**
 * The process callback for items found on the distributed queue structure.
 */
void DCOPY_process_objects(CIRCLE_handle* handle)
{
    char op[2048];
/*
    const char* DCOPY_op_string_table[] = {
        "TREEWALK",
        "COPY",
        "CLEANUP",
        "COMPARE"
    };
*/

    /* Pop an item off the queue */
    handle->dequeue(op);
    DCOPY_operation_t* opt = DCOPY_decode_operation(op);

/*
    LOG(DCOPY_LOG_DBG, "Performing operation `%s' on operand `%s' (`%d' remain on local queue).", \
        DCOPY_op_string_table[opt->code], opt->operand, handle->local_queue_size());
*/

    DCOPY_jump_table[opt->code](opt, handle);

    free(opt);
    return;
}

/* Unlink the destination file. */
void DCOPY_unlink_destination(DCOPY_operation_t* op)
{
    char dest_path_recursive[PATH_MAX];
    char dest_path_file_to_file[PATH_MAX];

    if(op->dest_base_appendix == NULL) {
        sprintf(dest_path_recursive, "%s/%s", \
                DCOPY_user_opts.dest_path, \
                op->operand + op->source_base_offset + 1);

        strncpy(dest_path_file_to_file, DCOPY_user_opts.dest_path, PATH_MAX);
    }
    else {
        sprintf(dest_path_recursive, "%s/%s/%s", \
                DCOPY_user_opts.dest_path, \
                op->dest_base_appendix, \
                op->operand + op->source_base_offset + 1);

        sprintf(dest_path_file_to_file, "%s/%s", \
                DCOPY_user_opts.dest_path, \
                op->dest_base_appendix);
    }

    if(unlink(dest_path_recursive) < 0) {
        LOG(DCOPY_LOG_DBG, "Failed to unlink recursive style destination. " \
            "%s", strerror(errno));
    }

    if(unlink(dest_path_file_to_file) < 0) {
        LOG(DCOPY_LOG_DBG, "Failed to unlink file-to-file style destination. " \
            "%s", strerror(errno));
    }

    return;
}

/* Open the input file. */
FILE* DCOPY_open_input_stream(DCOPY_operation_t* op)
{
    FILE* in_ptr = fopen(op->operand, "rb");

    if(in_ptr == NULL) {
        LOG(DCOPY_LOG_DBG, "Failed to open input file `%s'. %s", \
            op->operand, strerror(errno));
        /* Handle operation requeue in parent function. */
    }

    return in_ptr;
}

/*
 * Open the output file and return a stream.
 *
 * This function needs figure out if this is a file-to-file copy or a
 * recursive copy, then return a FILE* based on the result.
 */
FILE* DCOPY_open_output_stream(DCOPY_operation_t* op)
{
    char dest_path_recursive[PATH_MAX];
    char dest_path_file_to_file[PATH_MAX];

    FILE* out_ptr = NULL;

    if(op->dest_base_appendix == NULL) {
        sprintf(dest_path_recursive, "%s/%s", \
                DCOPY_user_opts.dest_path, \
                op->operand + op->source_base_offset + 1);

        strncpy(dest_path_file_to_file, DCOPY_user_opts.dest_path, PATH_MAX);
    }
    else {
        sprintf(dest_path_recursive, "%s/%s/%s", \
                DCOPY_user_opts.dest_path, \
                op->dest_base_appendix, \
                op->operand + op->source_base_offset + 1);

        sprintf(dest_path_file_to_file, "%s/%s", \
                DCOPY_user_opts.dest_path, \
                op->dest_base_appendix);
    }

/*
    LOG(DCOPY_LOG_DBG, "Opening destination path `%s' (recursive).", \
        dest_path_recursive);
*/

    /*
     * If we're recursive, we'll be doing this again and again, so try
     * recursive first. If it fails, then do the file-to-file.
     */
    if((out_ptr = fopen(dest_path_recursive, "rb")) == NULL) {

/*
        LOG(DCOPY_LOG_DBG, "Opening destination path `%s' " \
            "(file-to-file fallback).", \
            dest_path_file_to_file);
*/

        out_ptr = fopen(dest_path_file_to_file, "rb");
    }

    if(out_ptr == NULL) {
        LOG(DCOPY_LOG_DBG, "Failed to open destination path when comparing " \
            "from source `%s'. %s", op->operand, strerror(errno));

        /* Handle operation requeue in parent function. */
    }

    return out_ptr;
}

/*
 * Open the output file and return a descriptor.
 *
 * This function needs figure out if this is a file-to-file copy or a
 * recursive copy, then return an fd based on the result. The treewalk
 * stage has already setup a directory structure for us to use.
 */
int DCOPY_open_output_fd(DCOPY_operation_t* op)
{
    char dest_path_recursive[PATH_MAX];
    char dest_path_file_to_file[PATH_MAX];

    int out_fd = -1;

    if(op->dest_base_appendix == NULL) {
        sprintf(dest_path_recursive, "%s/%s", \
                DCOPY_user_opts.dest_path, \
                op->operand + op->source_base_offset + 1);

        strncpy(dest_path_file_to_file, DCOPY_user_opts.dest_path, PATH_MAX);
    }
    else {
        sprintf(dest_path_recursive, "%s/%s/%s", \
                DCOPY_user_opts.dest_path, \
                op->dest_base_appendix, \
                op->operand + op->source_base_offset + 1);

        sprintf(dest_path_file_to_file, "%s/%s", \
                DCOPY_user_opts.dest_path, \
                op->dest_base_appendix);
    }

/*
    LOG(DCOPY_LOG_DBG, "Opening destination path `%s' (recursive).", \
        dest_path_recursive);
*/

    /*
     * If we're recursive, we'll be doing this again and again, so try
     * recursive first. If it fails, then do the file-to-file.
     */
    if((out_fd = open(dest_path_recursive, O_RDWR | O_CREAT, S_IRWXU)) < 0) {
/*
        LOG(DCOPY_LOG_DBG, "Opening destination path `%s' " \
            "(file-to-file fallback).", \
            dest_path_file_to_file);
*/

        out_fd = open(dest_path_file_to_file, O_RDWR | O_CREAT, S_IRWXU);
    }

    if(out_fd < 0) {
        LOG(DCOPY_LOG_DBG, "Failed to open destination path when copying " \
            "from source `%s'. %s", op->operand, strerror(errno));

        /* Handle operation requeue in parent function. */
    }

    return out_fd;
}

/* EOF */
