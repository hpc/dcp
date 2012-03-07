/*
 * A distributed copy tool.
 */

#include "log.h"
#include "dcp.h"

#include "copy.h"
#include "filestat.h"
#include "checksum.h"

#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

/** The loglevel that this instance of dcopy will output. */
DCOPY_loglevel  DCOPY_debug_level;

/** Where we should keep statistics related to this file copy. */
DCOPY_statistics_t statistics;

/** Where we should store options specified by the user. */
DCOPY_options_t opts;

/** Where debug output should go. */
FILE* DCOPY_debug_stream;

/** What rank the current process is. */
int CIRCLE_global_rank;

void (*DCOPY_jump_table[4])(DCOPY_operation_t* op, CIRCLE_handle* handle);

char* DCOPY_encode_operation(DCOPY_operation_code_t op, int chunk, char* operand)
{
    char* result = (char*) malloc(sizeof(char) * 4096);
    sprintf(result, "%d:%d:%s", chunk, op, operand);
    return result;
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

void DCOPY_print_usage(char* prog_name)
{
    fprintf(stdout, "\n  Usage: %s [-dhvV] <source> ... [<special>:]<destination>\n\n", "foo");
    fprintf(stdout, "    Options:\n");
    fprintf(stdout, "      -d <level> - Set debug level to output.\n");
    fprintf(stdout, "      -h         - Print this usage message.\n");
    fprintf(stdout, "      -v         - Enable full verbose output.\n");
    fprintf(stdout, "      -V         - Print the version string.\n\n");
    fprintf(stdout, "    Field Descriptions:\n");
    fprintf(stdout, "      source      - A source path to copy from.\n");
    fprintf(stdout, "      destination - A destination path to copy to.\n");
    fprintf(stdout, "      special     - Not implemented, for future use.\n\n");
}

void DCOPY_epilogue(DCOPY_statistics_t* stats)
{
    double rate = stats->total_bytes_copied / end;

    time(&(stats->time_finished));

    char starttime_str[256];
    char endtime_str[256];

    struct tm* localstart = localtime(&(stats->time_started));
    struct tm* localend = localtime(&(stats->time_finished));

    strftime(starttime_str, 256, "%b-%d-%Y,%H:%M:%S", localstart);
    strftime(endtime_str, 256, "%b-%d-%Y,%H:%M:%S", localend);

    LOG(DCOPY_LOG_INFO, "Filecopy run started at: %s", starttime_str);
    LOG(DCOPY_LOG_INFO, "Filecopy run completed at: %s", endtime_str);
    LOG(DCOPY_LOG_INFO, "Filecopy total time (seconds) for this run: %f", difftime(stats->time_finished, stats->time_started));
    LOG(DCOPY_LOG_INFO, "Transfer rate: %ld bytes in %lf seconds.", stats->total_bytes_copied, end);
}

void DCOPY_prologue(void)
{
    DCOPY_jump_table[0] = DCOPY_do_copy;
    DCOPY_jump_table[1] = DCOPY_do_checksum;
    DCOPY_jump_table[2] = DCOPY_do_stat;

    time(&(statistics.time_started));
}

int main(int argc, char** argv)
{
    int c;
    int index;
    int option_index = 0;

    DCOPY_debug_stream = stdout;
    DCOPY_debug_level = DCOPY_LOG_DBG;

    static struct option long_options[] = {
        {"debug"  , required_argument, 0, 'd'},
        {"help"   , no_argument      , 0, 'h'},
        {"verbose", no_argument      , 0, 'v'},
        {"version", no_argument      , 0, 'V'},
        {0        , 0                , 0, 0  }
    };

    /* Parse options */
    while((c = getopt_long(argc, argv, "d:hvV", long_options, &option_index)) != -1) {
        switch(c) {
            case 'd':
                DCOPY_debug_level = atoi(optarg);
                LOG(DCOPY_LOG_DBG, "Verbose mode enabled.");
                break;

            case 'h':
                DCOPY_print_usage();
                exit(EXIT_SUCCESS);
                break;

            case 'v':
                DCOPY_debug_level = DCOPY_LOG_DBG;
                LOG(DCOPY_LOG_DBG, "Verbose mode enabled.");
                break;

            case 'V':
                DCOPY_print_version();
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

    /** Parse the remaining arguments that getopt didn't recognize. */
    if(!DCOPY_parse_path_args(&opts, argv, optind)) {
        LOG(DCOPY_LOG_ERR, "Unable to parse non-getopt options.");
        exit(EXIT_FAILURE);
    }

    /* Initialize state required for performing a copy. */
    DCOPY_prologue();

    /* Initialize our processing library. */
    CIRCLE_global_rank = CIRCLE_init(argc, argv, CIRCLE_DEFAULT_FLAGS);

    /* Initialize the processing library with the root object(s) callback. */
    CIRCLE_cb_create(&DCOPY_add_objects);

    /* Initialize processing library with the processing callback. */
    CIRCLE_cb_process(&DCOPY_process_objects);

    /* Grab a start time for benchmarking results. */
    statistics.start_time = CIRCLE_wtime();

    /* Perform the actual file copy. */
    CIRCLE_begin();

    /* Determine the end time for benchmarking results. */
    statistics.end_time = CIRCLE_wtime() - start;

    /* Let the processing library cleanup. */
    CIRCLE_finalize();

    /* Print the results to the user. */
    DCOPY_epilogue();

    exit(EXIT_SUCCESS);
}

/* EOF */
