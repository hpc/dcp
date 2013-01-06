/* See the file "COPYING" for the full license governing this code. */

#include "common.h"
#include "handle_args.h"

#include <stdlib.h>
#include <inttypes.h>
#include <string.h>

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

/**
 * Encode an operation code for use on the distributed queue structure.
 */
char* DCOPY_encode_operation(DCOPY_operation_code_t code, uint32_t chunk, \
                             char* operand, uint16_t source_base_offset, \
                             char* dest_base_appendix, uint64_t file_size)
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

    LOG(DCOPY_LOG_DBG, "Encoded operation is `%s'.", op);
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
    const char* DCOPY_op_string_table[] = {
        "TREEWALK",
        "COPY",
        "CLEANUP",
        "COMPARE"
    };

    /* Pop an item off the queue */
    handle->dequeue(op);
    DCOPY_operation_t* opt = DCOPY_decode_operation(op);

    LOG(DCOPY_LOG_DBG, "Performing operation `%s' on operand `%s' (`%d' remain on local queue).", \
        DCOPY_op_string_table[opt->code], opt->operand, handle->local_queue_size());

    DCOPY_jump_table[opt->code](opt, handle);

    free(opt);
    return;
}

/* EOF */
