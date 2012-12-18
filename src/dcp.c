/* See the file "COPYING" for the full license governing this code. */

#include <getopt.h>
#include <libgen.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "dcp.h"

#include "handle_args.h"
#include "compare.h"
#include "copy.h"
#include "stat_file.h"

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
void (*DCOPY_jump_table[4])(DCOPY_operation_t* op, CIRCLE_handle* handle);

/**
 * Encode an operation code for use on the distributed queue structure.
 */
char* DCOPY_encode_operation(DCOPY_operation_code_t op, uint32_t chunk, char* operand, uint16_t base_index)
{
    char* result = (char*) malloc(sizeof(char) * CIRCLE_MAX_STRING_LEN);
    sprintf(result, "%d:%d:%d:%s", chunk, op, base_index, operand);

    return result;
}

/**
 * Decode the operation code from a message on the distributed queue structure.
 */
DCOPY_operation_t* DCOPY_decode_operation(char* op)
{
    DCOPY_operation_t* ret = (DCOPY_operation_t*) malloc(sizeof(DCOPY_operation_t));

    ret->operand = (char*) malloc(sizeof(char) * PATH_MAX);

    ret->chunk = atoi(strtok(op, ":"));
    ret->code = atoi(strtok(NULL, ":"));
    ret->base_index = atoi(strtok(NULL, ":"));
    ret->operand = strtok(NULL, ":");

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
    char* DCOPY_op_string_table[] = {
        "COPY", "CHECKSUM", "STAT"
    };

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

/**
 * Print out information on the results of the file copy.
 */
void DCOPY_epilogue(void)
{
    double rel_time = DCOPY_statistics.wtime_ended - DCOPY_statistics.wtime_started;
    double rate = DCOPY_statistics.total_bytes_copied / rel_time;

    char starttime_str[256];
    char endtime_str[256];

    struct tm* localstart = localtime(&(DCOPY_statistics.time_started));
    struct tm* localend = localtime(&(DCOPY_statistics.time_ended));

    strftime(starttime_str, 256, "%b-%d-%Y,%H:%M:%S", localstart);
    strftime(endtime_str, 256, "%b-%d-%Y,%H:%M:%S", localend);

    LOG(DCOPY_LOG_INFO, "Filecopy run started at: %s", starttime_str);
    LOG(DCOPY_LOG_INFO, "Filecopy run completed at: %s", endtime_str);

    LOG(DCOPY_LOG_INFO, "Filecopy total time (seconds) for this run: %f", \
        difftime(DCOPY_statistics.time_started, DCOPY_statistics.time_ended));

    LOG(DCOPY_LOG_INFO, "Transfer rate: %lf bytes per second (%ld bytes in %lf seconds).", \
        rate, DCOPY_statistics.total_bytes_copied, rel_time);
}

/**
 * Print the current version.
 */
void DCOPY_print_version(char** argv)
{
    fprintf(stdout, "%s 0.0.0-pre0\n", argv[0]);
}

/**
 * Print a usage message.
 */
void DCOPY_print_usage(char** argv)
{
    fprintf(stdout, "usage: %s [CdfhpRrv] [--] source_file target_file\n", argv[0]);
    fprintf(stdout, "       %s [CdfhpRrv] [--] source_file ... target_directory\n", argv[0]);
}

int main(int argc, char** argv)
{
    int c;
    int option_index = 0;

    DCOPY_debug_stream = stdout;

    /* By default, don't skip the compare option. */
    DCOPY_user_opts.skip_compare = false;

    /* By default, show info log messages. */
    CIRCLE_loglevel CIRCLE_debug = CIRCLE_LOG_INFO;
    DCOPY_debug_level = DCOPY_LOG_DBG;

    /* By default, don't unlink destination files if an open() fails. */
    DCOPY_user_opts.force = false;

    /* By default, don't bother to preserve all attributes. */
    DCOPY_user_opts.preserve = false;

    /* By default, don't attempt any type of recursion. */
    DCOPY_user_opts.recursive = false;
    DCOPY_user_opts.recursive_unspecified = false;

    static struct option long_options[] = {
        {"skip-compare"         , no_argument      , 0, 'C'},
        {"debug"                , required_argument, 0, 'd'},
        {"force"                , no_argument      , 0, 'f'},
        {"help"                 , no_argument      , 0, 'h'},
        {"preserve"             , no_argument      , 0, 'p'},
        {"recursive"            , no_argument      , 0, 'R'},
        {"recursive-unspecified", no_argument      , 0, 'r'},
        {"version"              , no_argument      , 0, 'v'},
        {0                      , 0                , 0, 0  }
    };

    /* Parse options */
    while((c = getopt_long(argc, argv, "Cd:fhpRrv", long_options, &option_index)) != -1) {
        switch(c) {

            case 'C':
                DCOPY_user_opts.skip_compare = true;
                LOG(DCOPY_LOG_INFO, "Skipping the comparison stage, this may result in file corruption.");
                break;

            case 'd':

                if(strncmp(optarg, "fatal", 5)) {
                    CIRCLE_debug = CIRCLE_LOG_FATAL;
                    DCOPY_debug_level = DCOPY_LOG_FATAL;
                    LOG(DCOPY_LOG_INFO, "Debug level set to: fatal");

                }
                else if(strncmp(optarg, "err", 3)) {
                    CIRCLE_debug = CIRCLE_LOG_ERR;
                    DCOPY_debug_level = DCOPY_LOG_ERR;
                    LOG(DCOPY_LOG_INFO, "Debug level set to: errors");

                }
                else if(strncmp(optarg, "warn", 4)) {
                    CIRCLE_debug = CIRCLE_LOG_WARN;
                    DCOPY_debug_level = DCOPY_LOG_WARN;
                    LOG(DCOPY_LOG_INFO, "Debug level set to: warnings");

                }
                else if(strncmp(optarg, "info", 4)) {
                    CIRCLE_debug = CIRCLE_LOG_INFO;
                    DCOPY_debug_level = DCOPY_LOG_INFO;
                    LOG(DCOPY_LOG_INFO, "Debug level set to: info");

                }
                else if(strncmp(optarg, "dbg", 4)) {
                    CIRCLE_debug = CIRCLE_LOG_DBG;
                    DCOPY_debug_level = DCOPY_LOG_DBG;
                    LOG(DCOPY_LOG_INFO, "Debug level set to: debug");

                }
                else {
                    LOG(DCOPY_LOG_INFO, "Debug level `%s' not recognized. Defaulting to `info'.", optarg);
                }

                break;

            case 'f':
                DCOPY_user_opts.force = true;
                LOG(DCOPY_LOG_INFO, "Unlinking destionation file if create or truncate fails.");
                break;

            case 'h':
                DCOPY_print_usage(argv);
                exit(EXIT_SUCCESS);
                break;

            case 'p':
                DCOPY_user_opts.preserve = true;
                LOG(DCOPY_LOG_INFO, "Preserving file attributes.");
                break;

            case 'R':
                DCOPY_user_opts.recursive = true;
                LOG(DCOPY_LOG_INFO, "Performing correct recursion.");
                break;

            case 'r':
                DCOPY_user_opts.recursive_unspecified = true;
                LOG(DCOPY_LOG_INFO, "Performing recursion while ignoring special files.");
                break;

            case 'v':
                DCOPY_print_version(argv);
                exit(EXIT_SUCCESS);
                break;

            case '?':

                if(optopt == 'd') {
                    DCOPY_print_usage(argv);
                    fprintf(stderr, "Option -%c requires an argument.\n", optopt);
                }
                else if(isprint(optopt)) {
                    DCOPY_print_usage(argv);
                    fprintf(stderr, "Unknown option `-%c'.\n", optopt);
                }
                else {
                    DCOPY_print_usage(argv);
                    fprintf(stderr,
                            "Unknown option character `\\x%x'.\n",
                            optopt);
                }

                exit(EXIT_FAILURE);
                break;
        }
    }

    /** Parse the source and destination paths. */
    DCOPY_parse_path_args(argv, optind, argc);

    /* Initialize our jump table for core operations. */
    DCOPY_jump_table[0] = DCOPY_do_copy;
    DCOPY_jump_table[1] = DCOPY_do_compare;
    DCOPY_jump_table[2] = DCOPY_do_stat;

    /* Initialize our processing library and related callbacks. */
    CIRCLE_global_rank = CIRCLE_init(argc, argv, CIRCLE_DEFAULT_FLAGS);
    CIRCLE_cb_create(&DCOPY_add_objects);
    CIRCLE_cb_process(&DCOPY_process_objects);

    /* Set the log level for the processing library. */
    CIRCLE_enable_logging(CIRCLE_debug);

    /* Grab a relative and actual start time for the epilogue. */
    time(&(DCOPY_statistics.time_started));
    DCOPY_statistics.wtime_started = CIRCLE_wtime();

    /* Perform the actual file copy. */
    CIRCLE_begin();

    /* Determine the actual and relative end time for the epilogue. */
    DCOPY_statistics.wtime_ended = CIRCLE_wtime();
    time(&(DCOPY_statistics.time_ended));

    /* Let the processing library cleanup. */
    CIRCLE_finalize();

    /* Print the results to the user. */
    DCOPY_epilogue();

    exit(EXIT_SUCCESS);
}

/* EOF */
