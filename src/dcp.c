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
#include <inttypes.h>

/** Options specified by the user. */
extern DCOPY_options_t DCOPY_user_opts;

/** Statistics to gather for summary output. */
extern DCOPY_statistics_t DCOPY_statistics;

/*
 * A table of function pointers used for core operation. These functions
 * perform each stage of the file copy operation: "treewalk", "copy",
 * "cleanup", and "compare". File operations are usually passed through
 * all stages in a linear fashion unless a failure occurs. If a failure
 * occurs, the operation may be passed to a previous stage.
 */
extern void (*DCOPY_jump_table[5])(DCOPY_operation_t* op, \
                                   CIRCLE_handle* handle);

/* iterate through linked list of files and set timestamps,
 * starting from deepest level and working backwards */
static void DCOPY_set_timestamps()
{
    const DCOPY_stat_elem_t* elem;

    /* get max depth across all procs */
    int max_depth;
    int depth = -1;
    elem = DCOPY_list_head;
    while (elem != NULL) {
        if (elem->depth > depth) {
            depth = elem->depth;
        }
        elem = elem->next;
    }
    MPI_Allreduce(&depth, &max_depth, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);

    /* now set timestamps on files starting from deepest level */
    for (depth = max_depth; depth > 0; depth--) {
        /* cycle through our list of files and set timestamps
         * for each one at this level */
        elem = DCOPY_list_head;
        while (elem != NULL) {
            if (elem->depth == depth) {
                DCOPY_copy_timestamps(elem->sb, elem->file);
            }
            elem = elem->next;
        }
        
        /* wait for all procs to finish before we start
         * with files at next level */
        MPI_Barrier(MPI_COMM_WORLD);
    }

    return;
}

static int64_t DCOPY_sum_int64(int64_t val)
{
    long long val_ull = (long long) val;
    long long sum;
    MPI_Allreduce(&val_ull, &sum, 1, MPI_LONG_LONG, MPI_SUM, MPI_COMM_WORLD);
    return (int64_t) sum;
}

/**
 * Print out information on the results of the file copy.
 */
void DCOPY_epilogue(void)
{
    double rel_time = DCOPY_statistics.wtime_ended - \
                      DCOPY_statistics.wtime_started;
    double rate = (double)DCOPY_statistics.total_bytes_copied / rel_time;
    LOG(DCOPY_LOG_INFO, "Rank %d: Transfer rate is `%.0lf' bytes per second " \
        "(`%.3" PRId64 "' bytes in `%.3lf' seconds).", \
        CIRCLE_global_rank, rate, DCOPY_statistics.total_bytes_copied, rel_time);

    int64_t agg_copied = DCOPY_sum_int64(DCOPY_statistics.total_bytes_copied);
    double agg_rate = (double)agg_copied / rel_time;

    if(CIRCLE_global_rank == 0) {
        char starttime_str[256];
        char endtime_str[256];

        struct tm* localstart = localtime(&(DCOPY_statistics.time_started));
        struct tm* localend = localtime(&(DCOPY_statistics.time_ended));

        strftime(starttime_str, 256, "%b-%d-%Y,%H:%M:%S", localstart);
        strftime(endtime_str, 256, "%b-%d-%Y,%H:%M:%S", localend);

        LOG(DCOPY_LOG_INFO, "Filecopy run started at `%s'.", starttime_str);
        LOG(DCOPY_LOG_INFO, "Filecopy run completed at `%s'.", endtime_str);

        LOG(DCOPY_LOG_INFO, "Aggregate transfer rate is `%.0lf' bytes per second " \
            "(`%.3" PRId64 "' bytes in `%.3lf' seconds).", \
            agg_rate, agg_copied, rel_time);
    }

    /* free each source path and array of source path pointers */
    if(DCOPY_user_opts.src_path != NULL) {
        int i;

        for(i = 0; i < DCOPY_user_opts.num_src_paths; i++) {
            free(DCOPY_user_opts.src_path[i]);
        }

        free(DCOPY_user_opts.src_path);
        DCOPY_user_opts.src_path = NULL;
    }

    /* free destination path */
    if(DCOPY_user_opts.dest_path != NULL) {
        free(DCOPY_user_opts.dest_path);
        DCOPY_user_opts.dest_path = NULL;
    }

    return;
}

/**
 * Print the current version.
 */
void DCOPY_print_version()
{
    fprintf(stdout, "%s-%s\n", PACKAGE_NAME, PACKAGE_VERSION);
}

/**
 * Print a usage message.
 */
void DCOPY_print_usage(char** argv)
{
    printf("usage: %s [cCdfhpRrv] [--] source_file target_file\n" \
           "       %s [cCdfhpRrv] [--] source_file ... target_directory\n", \
           argv[0], argv[0]);
    fflush(stdout);
}

int main(int argc, \
         char** argv)
{
    int c;
    int option_index = 0;

    MPI_Init(&argc, &argv);

    /* Initialize our processing library and related callbacks. */
    /* This is a bit of chicken-and-egg problem, because we'd like
     * to have our rank to filter output messages below but we might
     * also want to set different libcircle flags based on command line
     * options -- for now just pass in the default flags */
    CIRCLE_global_rank = CIRCLE_init(argc, argv, CIRCLE_DEFAULT_FLAGS);
    CIRCLE_cb_create(&DCOPY_add_objects);
    CIRCLE_cb_process(&DCOPY_process_objects);

    DCOPY_debug_stream = stdout;

    /* By default, don't perform a conditional copy. */
    DCOPY_user_opts.conditional = false;

    /* By default, don't skip the compare option. */
    DCOPY_user_opts.skip_compare = false;

    /* By default, show info log messages. */
    CIRCLE_loglevel CIRCLE_debug = CIRCLE_LOG_INFO;
    DCOPY_debug_level = DCOPY_LOG_INFO;

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

                if(CIRCLE_global_rank == 0) {
                    LOG(DCOPY_LOG_INFO, "Performing a conditional copy.");
                }

                break;

            case 'C':
                DCOPY_user_opts.skip_compare = true;

                if(CIRCLE_global_rank == 0) {
                    LOG(DCOPY_LOG_INFO, "Skipping the comparison stage " \
                        "(may result in corruption).");
                }

                break;

            case 'd':

                if(strncmp(optarg, "fatal", 5)) {
                    CIRCLE_debug = CIRCLE_LOG_FATAL;
                    DCOPY_debug_level = DCOPY_LOG_FATAL;

                    if(CIRCLE_global_rank == 0) {
                        LOG(DCOPY_LOG_INFO, "Debug level set to: fatal");
                    }

                }
                else if(strncmp(optarg, "err", 3)) {
                    CIRCLE_debug = CIRCLE_LOG_ERR;
                    DCOPY_debug_level = DCOPY_LOG_ERR;

                    if(CIRCLE_global_rank == 0) {
                        LOG(DCOPY_LOG_INFO, "Debug level set to: errors");
                    }

                }
                else if(strncmp(optarg, "warn", 4)) {
                    CIRCLE_debug = CIRCLE_LOG_WARN;
                    DCOPY_debug_level = DCOPY_LOG_WARN;

                    if(CIRCLE_global_rank == 0) {
                        LOG(DCOPY_LOG_INFO, "Debug level set to: warnings");
                    }

                }
                else if(strncmp(optarg, "info", 4)) {
                    CIRCLE_debug = CIRCLE_LOG_INFO;
                    DCOPY_debug_level = DCOPY_LOG_INFO;

                    if(CIRCLE_global_rank == 0) {
                        LOG(DCOPY_LOG_INFO, "Debug level set to: info");
                    }

                }
                else if(strncmp(optarg, "dbg", 4)) {
                    CIRCLE_debug = CIRCLE_LOG_DBG;
                    DCOPY_debug_level = DCOPY_LOG_DBG;

                    if(CIRCLE_global_rank == 0) {
                        LOG(DCOPY_LOG_INFO, "Debug level set to: debug");
                    }

                }
                else {
                    if(CIRCLE_global_rank == 0) {
                        LOG(DCOPY_LOG_INFO, "Debug level `%s' not recognized. " \
                            "Defaulting to `info'.", optarg);
                    }
                }

                break;

            case 'f':
                DCOPY_user_opts.force = true;

                if(CIRCLE_global_rank == 0) {
                    LOG(DCOPY_LOG_INFO, "Deleting destination on errors.");
                }

                break;

            case 'h':

                if(CIRCLE_global_rank == 0) {
                    DCOPY_print_usage(argv);
                }

                DCOPY_exit(EXIT_SUCCESS);
                break;

            case 'p':
                DCOPY_user_opts.preserve = true;

                if(CIRCLE_global_rank == 0) {
                    LOG(DCOPY_LOG_INFO, "Preserving file attributes.");
                }

                break;

            case 'R':
                DCOPY_user_opts.recursive = true;

                if(CIRCLE_global_rank == 0) {
                    LOG(DCOPY_LOG_INFO, "Performing correct recursion.");
                    LOG(DCOPY_LOG_WARN, "Warning, only files and directories are implemented.");
                }

                break;

            case 'r':
                DCOPY_user_opts.recursive_unspecified = true;

                if(CIRCLE_global_rank == 0) {
                    LOG(DCOPY_LOG_INFO, "Performing recursion. " \
                        "Ignoring special files.");
                }

                break;

            case 'U':
                DCOPY_user_opts.reliable_filesystem = false;

                if(CIRCLE_global_rank == 0) {
                    LOG(DCOPY_LOG_INFO, "Unreliable filesystem specified. " \
                        "Retry mode enabled.");
                }

                break;

            case 'v':

                if(CIRCLE_global_rank == 0) {
                    DCOPY_print_version();
                }

                DCOPY_exit(EXIT_SUCCESS);
                break;

            case '?':
            default:

                if(CIRCLE_global_rank == 0) {
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
                }

                DCOPY_exit(EXIT_FAILURE);
                break;
        }
    }

    /** Parse the source and destination paths. */
    DCOPY_parse_path_args(argv, optind, argc);

    /* initialize linked list of stat objects */
    DCOPY_list_head  = NULL;
    DCOPY_list_tail  = NULL;

    /* Initialize our jump table for core operations. */
    DCOPY_jump_table[TREEWALK] = DCOPY_do_treewalk;
    DCOPY_jump_table[COPY]     = DCOPY_do_copy;
    DCOPY_jump_table[CLEANUP]  = DCOPY_do_cleanup;
    DCOPY_jump_table[COMPARE]  = DCOPY_do_compare;

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

    /* set timestamps if needed */
    if (DCOPY_user_opts.preserve) {
        DCOPY_set_timestamps();
    }

    /* free list of stat objects */
    DCOPY_stat_elem_t* current = DCOPY_list_head;
    while (current != NULL) {
        DCOPY_stat_elem_t* next = current->next;
        free(current->file);
        free(current->sb);
        free(current);
        current = next;
    }

    /* Print the results to the user. */
    DCOPY_epilogue();

    DCOPY_exit(EXIT_SUCCESS);
}

/* EOF */
