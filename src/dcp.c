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

/* iterate through linked list of files and set ownership, timestamps, and permissions
 * starting from deepest level and working backwards */
static void DCOPY_set_metadata()
{
    const DCOPY_stat_elem_t* elem;

    if (CIRCLE_global_rank == 0) {
        if(DCOPY_user_opts.preserve) {
            LOG(DCOPY_LOG_INFO, "Setting ownership, permissions, and timestamps.");
        }
        else {
            LOG(DCOPY_LOG_INFO, "Fixing permissions.");
        }
    }

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
                if(DCOPY_user_opts.preserve) {
                    DCOPY_copy_ownership(elem->sb, elem->file);
                    DCOPY_copy_permissions(elem->sb, elem->file);
                    DCOPY_copy_timestamps(elem->sb, elem->file);
                }
                else {
                    /* TODO: set permissions based on source permissons
                     * masked by umask */
                    DCOPY_copy_permissions(elem->sb, elem->file);
                }
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
    int64_t agg_dirs   = DCOPY_sum_int64(DCOPY_statistics.total_dirs);
    int64_t agg_files  = DCOPY_sum_int64(DCOPY_statistics.total_files);
    int64_t agg_links  = DCOPY_sum_int64(DCOPY_statistics.total_links);
    int64_t agg_size   = DCOPY_sum_int64(DCOPY_statistics.total_size);
    int64_t agg_copied = DCOPY_sum_int64(DCOPY_statistics.total_bytes_copied);
    double agg_rate = (double)agg_copied / rel_time;

    if(CIRCLE_global_rank == 0) {
        char starttime_str[256];
        struct tm* localstart = localtime(&(DCOPY_statistics.time_started));
        strftime(starttime_str, 256, "%b-%d-%Y,%H:%M:%S", localstart);

        char endtime_str[256];
        struct tm* localend = localtime(&(DCOPY_statistics.time_ended));
        strftime(endtime_str, 256, "%b-%d-%Y,%H:%M:%S", localend);

        int64_t agg_items = agg_dirs + agg_files + agg_links;

        /* convert size to units */
        double agg_size_tmp;
        const char* agg_size_units;
        bayer_format_bytes((uint64_t)agg_size, &agg_size_tmp, &agg_size_units);

        /* convert bandwidth to units */
        double agg_rate_tmp;
        const char* agg_rate_units;
        bayer_format_bw(agg_rate, &agg_rate_tmp, &agg_rate_units);

        LOG(DCOPY_LOG_INFO, "Started: %s", starttime_str);
        LOG(DCOPY_LOG_INFO, "Completed: %s", endtime_str);
        LOG(DCOPY_LOG_INFO, "Seconds: %.3lf", rel_time);
        LOG(DCOPY_LOG_INFO, "Items: %" PRId64, agg_items);
        LOG(DCOPY_LOG_INFO, "Directories: %" PRId64, agg_dirs);
        LOG(DCOPY_LOG_INFO, "Files: %" PRId64, agg_files);
        LOG(DCOPY_LOG_INFO, "Links: %" PRId64, agg_links);
        LOG(DCOPY_LOG_INFO, "Data: %.3lf %s (%" PRId64 " bytes)",
            agg_size_tmp, agg_size_units, agg_size);

        LOG(DCOPY_LOG_INFO, "Rate: %.3lf %s " \
            "(%.3" PRId64 " bytes in %.3lf seconds)", \
            agg_rate_tmp, agg_rate_units, agg_copied, rel_time);
    }

    /* free memory allocated to parse user params */
    DCOPY_free_path_args();

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
    printf("usage: %s [cdfhpv] [--] source target\n" \
           "       %s [cdfhpv] [--] source ... target_directory\n", \
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

    /* Initialize statistics */
    DCOPY_statistics.total_dirs  = 0;
    DCOPY_statistics.total_files = 0;
    DCOPY_statistics.total_links = 0;
    DCOPY_statistics.total_size  = 0;
    DCOPY_statistics.total_bytes_copied = 0;

    /* By default, skip the compare option. */
    DCOPY_user_opts.compare = false;

    /* By default, show info log messages. */
    CIRCLE_loglevel CIRCLE_debug = CIRCLE_LOG_INFO;
    DCOPY_debug_level = DCOPY_LOG_INFO;

    /* By default, don't unlink destination files if an open() fails. */
    DCOPY_user_opts.force = false;

    /* By default, don't bother to preserve all attributes. */
    DCOPY_user_opts.preserve = false;

    /* By default, assume the filesystem is reliable (exit on errors). */
    DCOPY_user_opts.reliable_filesystem = true;

    /* Set default chunk size */
    DCOPY_user_opts.chunk_size = DCOPY_CHUNK_SIZE;

    /* Set default block size */
    DCOPY_user_opts.block_size = FD_BLOCK_SIZE;

    static struct option long_options[] = {
        {"skip-compare"         , no_argument      , 0, 'c'},
        {"debug"                , required_argument, 0, 'd'},
        {"force"                , no_argument      , 0, 'f'},
        {"help"                 , no_argument      , 0, 'h'},
        {"preserve"             , no_argument      , 0, 'p'},
        {"unreliable-filesystem", no_argument      , 0, 'U'},
        {"version"              , no_argument      , 0, 'v'},
        {0                      , 0                , 0, 0  }
    };

    /* Parse options */
    while((c = getopt_long(argc, argv, "cd:fhpUv", \
                           long_options, &option_index)) != -1) {
        switch(c) {

            case 'c':
                DCOPY_user_opts.compare = true;

                if(CIRCLE_global_rank == 0) {
                    LOG(DCOPY_LOG_INFO, "Compare source and destination " \
			"after copy to detect corruption.");
                }

                break;

            case 'd':

                if(strncmp(optarg, "fatal", 5) == 0) {
                    CIRCLE_debug = CIRCLE_LOG_FATAL;
                    DCOPY_debug_level = DCOPY_LOG_FATAL;

                    if(CIRCLE_global_rank == 0) {
                        LOG(DCOPY_LOG_INFO, "Debug level set to: fatal");
                    }

                }
                else if(strncmp(optarg, "err", 3) == 0) {
                    CIRCLE_debug = CIRCLE_LOG_ERR;
                    DCOPY_debug_level = DCOPY_LOG_ERR;

                    if(CIRCLE_global_rank == 0) {
                        LOG(DCOPY_LOG_INFO, "Debug level set to: errors");
                    }

                }
                else if(strncmp(optarg, "warn", 4) == 0) {
                    CIRCLE_debug = CIRCLE_LOG_WARN;
                    DCOPY_debug_level = DCOPY_LOG_WARN;

                    if(CIRCLE_global_rank == 0) {
                        LOG(DCOPY_LOG_INFO, "Debug level set to: warnings");
                    }

                }
                else if(strncmp(optarg, "info", 4) == 0) {
                    CIRCLE_debug = CIRCLE_LOG_INFO;
                    DCOPY_debug_level = DCOPY_LOG_INFO;

                    if(CIRCLE_global_rank == 0) {
                        LOG(DCOPY_LOG_INFO, "Debug level set to: info");
                    }

                }
                else if(strncmp(optarg, "dbg", 3) == 0) {
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

    /* set permissions, ownership, and timestamps if needed */
    DCOPY_set_metadata();

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
