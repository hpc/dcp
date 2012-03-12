/* See the file "COPYING" for the full license governing this code. */

#include <getopt.h>
#include <libgen.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "dcp.h"

#include "argparse.h"
#include "checksum.h"
#include "copy.h"
#include "filestat.h"

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
    char** src_p = DCOPY_user_opts.src_path;

    while(*src_p != NULL) {
        char* op = DCOPY_encode_operation(STAT, 0, *src_p, strlen(*src_p));
        handle->enqueue(op);

        free(op);
        src_p++;
    }
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
    fprintf(stdout, "\n  Usage: %s [-dhvmVP] <source> ... [<special>:]<destination>\n\n", argv[0]);
    fprintf(stdout, "    Options:\n");
    fprintf(stdout, "      -d <level> - Set debug level to output.\n");
    fprintf(stdout, "      -h         - Print this usage message.\n");
    fprintf(stdout, "      -v         - Enable full verbose output.\n");
    fprintf(stdout, "      -m         - Merge source with destination directory.\n");
    fprintf(stdout, "      -V         - Print the version string.\n");
    fprintf(stdout, "      -P         - DANGEROUS, skip the compare stage.\n\n");
    fprintf(stdout, "    Field Descriptions:\n");
    fprintf(stdout, "      source      - A source path to copy from.\n");
    fprintf(stdout, "      destination - A destination path to copy to.\n");
    fprintf(stdout, "      special     - Not implemented, for future use.\n\n");
}

int main(int argc, char** argv)
{
    int c;
    int option_index = 0;

    DCOPY_debug_stream = stdout;
    DCOPY_debug_level = DCOPY_LOG_INFO;

    CIRCLE_loglevel CIRCLE_debug = CIRCLE_LOG_FATAL;

    /* By default, don't skip the compare option. */
    DCOPY_user_opts.skip_compare = false;

    /* Make sure the destination stat cache is empty. */
    DCOPY_user_opts.dest_stat_exists = false;

    static struct option long_options[] = {
        {"debug"       , required_argument, 0, 'd'},
        {"help"        , no_argument      , 0, 'h'},
        {"verbose"     , no_argument      , 0, 'v'},
        {"merge"       , no_argument      , 0, 'm'},
        {"version"     , no_argument      , 0, 'V'},
        {"skip-compare", no_argument      , 0, 'P'},
        {0             , 0                , 0, 0  }
    };

    /* Parse options */
    while((c = getopt_long(argc, argv, "d:hvmVP", long_options, &option_index)) != -1) {
        switch(c) {
            case 'd':
                DCOPY_debug_level = atoi(optarg);
                CIRCLE_debug = (enum CIRCLE_loglevel)DCOPY_debug_level;
                LOG(DCOPY_LOG_DBG, "Verbose mode enabled.");
                break;

            case 'h':
                DCOPY_print_usage(argv);
                exit(EXIT_SUCCESS);
                break;

            case 'v':
                DCOPY_debug_level = DCOPY_LOG_DBG;
                CIRCLE_debug = CIRCLE_LOG_DBG;
                LOG(DCOPY_LOG_DBG, "Verbose mode enabled.");
                break;

            case 'm':
                DCOPY_user_opts.merge_into_dest = true;
                LOG(DCOPY_LOG_INFO, "Merging source(s) into the destination directory.");
                break;

            case 'V':
                DCOPY_print_version(argv);
                exit(EXIT_SUCCESS);
                break;

            case 'P':
                DCOPY_user_opts.skip_compare = true;
                LOG(DCOPY_LOG_INFO, "Skipping the comparison stage, this may result in file corruption.");
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
    DCOPY_jump_table[1] = DCOPY_do_checksum;
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
