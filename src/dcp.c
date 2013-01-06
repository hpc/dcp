/* See the file "COPYING" for the full license governing this code. */

#include "dcp.h"

#include "handle_args.h"
#include "treewalk.h"
#include "copy.h"
#include "cleanup.h"
#include "compare.h"

#include <getopt.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/** Options specified by the user. */
extern DCOPY_options_t DCOPY_user_opts;

/** Statistics to gather for summary output. */
extern DCOPY_statistics_t DCOPY_statistics;

/** A table of function pointers used for core operation. */
extern void (*DCOPY_jump_table[5])(DCOPY_operation_t* op, \
                                   CIRCLE_handle* handle);

/**
 * Print out information on the results of the file copy.
 */
void DCOPY_epilogue(void)
{
    double rel_time = DCOPY_statistics.wtime_ended - \
                      DCOPY_statistics.wtime_started;
    double rate = (double)DCOPY_statistics.total_bytes_copied / rel_time;

    char starttime_str[256];
    char endtime_str[256];

    struct tm* localstart = localtime(&(DCOPY_statistics.time_started));
    struct tm* localend = localtime(&(DCOPY_statistics.time_ended));

    strftime(starttime_str, 256, "%b-%d-%Y,%H:%M:%S", localstart);
    strftime(endtime_str, 256, "%b-%d-%Y,%H:%M:%S", localend);

    LOG(DCOPY_LOG_INFO, "Filecopy run started at `%s'.", starttime_str);
    LOG(DCOPY_LOG_INFO, "Filecopy run completed at `%s'.", endtime_str);

    LOG(DCOPY_LOG_INFO, "Transfer rate is `%.0lf' bytes per second " \
        "(`%.3ld' bytes in `%.3lf' seconds).", \
        rate, DCOPY_statistics.total_bytes_copied, rel_time);
}

/**
 * Print the current version.
 */
void DCOPY_print_version()
{
    fprintf(stdout, "%s-%s <%s>\n", \
            PACKAGE_NAME, PACKAGE_VERSION, PACKAGE_URL);
}

/**
 * Print a usage message.
 */
void DCOPY_print_usage(char** argv)
{
    printf("usage: %s [cCdfhpRrv] [--] source_file target_file\n" \
           "       %s [cCdfhpRrv] [--] source_file ... target_directory\n", \
           argv[0], argv[0]);
}

int main(int argc, char** argv)
{
    int c;
    int option_index = 0;

    DCOPY_debug_stream = stdout;

    /* By default, don't perform a conditional copy. */
    DCOPY_user_opts.conditional = false;

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

    /* By default, assume the filesystem is reliable (exit on errors). */
    DCOPY_user_opts.reliable_filesystem = true;

    static struct option long_options[] = {
        {"conditional"          , no_argument      , 0, 'c'},
        {"skip-compare"         , no_argument      , 0, 'C'},
        {"debug"                , required_argument, 0, 'd'},
        {"force"                , no_argument      , 0, 'f'},
        {"help"                 , no_argument      , 0, 'h'},
        {"preserve"             , no_argument      , 0, 'p'},
        {"recursive"            , no_argument      , 0, 'R'},
        {"recursive-unspecified", no_argument      , 0, 'r'},
        {"unreliable-filesystem", no_argument      , 0, 'U'},
        {"version"              , no_argument      , 0, 'v'},
        {0                      , 0                , 0, 0  }
    };

    /* Parse options */
    while((c = getopt_long(argc, argv, "cCd:fhpRrUv", \
                           long_options, &option_index)) != -1) {
        switch(c) {

            case 'c':
                DCOPY_user_opts.conditional = true;
                LOG(DCOPY_LOG_INFO, "Performing a conditional copy.");
                break;

            case 'C':
                DCOPY_user_opts.skip_compare = true;
                LOG(DCOPY_LOG_INFO, "Skipping the comparison stage " \
                    "(may result in corruption).");
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
                    LOG(DCOPY_LOG_INFO, "Debug level `%s' not recognized. " \
                        "Defaulting to `info'.", optarg);
                }

                break;

            case 'f':

                DCOPY_user_opts.force = true;
                LOG(DCOPY_LOG_INFO, "Deleting destination on errors.");
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
                LOG(DCOPY_LOG_WARN, "Warning, only files and directories are implemented.");
                break;

            case 'r':

                DCOPY_user_opts.recursive_unspecified = true;
                LOG(DCOPY_LOG_INFO, "Performing recursion. " \
                    "Ignoring special files.");
                break;

            case 'U':

                DCOPY_user_opts.reliable_filesystem = false;
                LOG(DCOPY_LOG_INFO, "Unreliable filesystem specified. " \
                    "Retry mode enabled.");
                break;

            case 'v':
                DCOPY_print_version();
                exit(EXIT_SUCCESS);
                break;

            case '?':
            default:

                if(optopt == 'd') {
                    DCOPY_print_usage(argv);
                    fprintf(stderr, "Option -%c requires an argument.\n", \
                            optopt);
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
    DCOPY_jump_table[0] = DCOPY_do_treewalk;
    DCOPY_jump_table[1] = DCOPY_do_copy;
    DCOPY_jump_table[2] = DCOPY_do_cleanup;
    DCOPY_jump_table[3] = DCOPY_do_compare;

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
