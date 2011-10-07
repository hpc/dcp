#include <libcircle.h> 
#include "log.h"
#include "dcp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <fcntl.h>
#include <dirent.h>
#include <libgen.h>
#include <sys/stat.h>
#include <errno.h>
#include <ctype.h>

int DCOPY_global_rank;
FILE *DCOPY_debug_stream;
DCOPY_loglevel DCOPY_debug_level;

char *DCOPY_SRC_PATH;
char *DCOPY_DEST_PATH;

void
DCOPY_start(CIRCLE_handle *handle)
{
    handle->enqueue(DCOPY_SRC_PATH);
    mkdir(DCOPY_DEST_PATH, 0755);
    DCOPY_DEST_PATH = realpath(DCOPY_DEST_PATH, NULL);
}

void
DCOPY_copy(CIRCLE_handle *handle)
{
    char item[CIRCLE_MAX_STRING_LEN];
    struct stat st;

    handle->dequeue(item);

    if(lstat(item, &st) < 0)
    {
        LOG(DCOPY_LOG_ERR, "Error: Couldn't stat: %s", item);
    }
    else if(S_ISDIR(st.st_mode) && !(S_ISLNK(st.st_mode)))
    {
        DCOPY_handle_directory(handle, &st, item);
    }
    else if(S_ISREG(st.st_mode))
    {
        DCOPY_handle_regular(handle, &st, item);
    }
}

void
DCOPY_handle_regular(CIRCLE_handle *handle, struct stat *st, char *name)
{
    FILE *fp;
    char cmd[CIRCLE_MAX_STRING_LEN + 10];
    char out[1035];

    sprintf(cmd, "cp %s %s", name, "FOOBAR");

    LOG(DCOPY_LOG_DBG, "Running cmd: %s", cmd);
    fp = popen(cmd, "r");
    if (fp == NULL) {
        printf("Failed to run command\n" );
    }

    while (fgets(out, sizeof(out) - 1, fp) != NULL) {
        printf("%s", out);
    }

    pclose(fp);
}

void
DCOPY_handle_directory(CIRCLE_handle *handle, struct stat *st, char *name)
{
    char stat_temp[CIRCLE_MAX_STRING_LEN];
    struct dirent *current_ent;
    DIR *current_dir;

    current_dir = opendir(name);

    if(!current_dir)
    {
        LOG(DCOPY_LOG_ERR, "Unable to open dir");
    }
    else
    {
        /* Since it's quick, just create the directory at the destination. */
        char * new_dir_name = malloc(4096);
        sprintf(new_dir_name, "%s/%s", DCOPY_DEST_PATH, name);
        LOG(DCOPY_LOG_DBG, "Creating directory with name: %s", new_dir_name);
        mkdir(new_dir_name, st->st_mode);
        free(new_dir_name);

        /* Read in each directory entry */
        while((current_ent = readdir(current_dir)) != NULL)
        {
            /* We don't care about . or .. */
            if((strncmp(current_ent->d_name,".",2)) && (strncmp(current_ent->d_name,"..",3)))
            {
                strcpy(stat_temp, name);
                strcat(stat_temp, "/");
                strcat(stat_temp, current_ent->d_name);

                handle->enqueue(&stat_temp[0]);
            }
        }
    }
    closedir(current_dir);
}

void
print_usage(char *prog)
{
    fprintf(stdout, "Usage: %s -s <source> -d <destination>\n", prog);
}

int
main (int argc, char **argv)
{
    int index;
    int c;

    int src_flag = 0;
    int dest_flag = 0;

    DCOPY_debug_stream = stdout;
    DCOPY_debug_level  = DCOPY_LOG_DBG;
     
    opterr = 0;
    while((c = getopt(argc, argv, "s:d:")) != -1)
    {
        switch(c)
        {
            case 'd':
                DCOPY_DEST_PATH = optarg;
                dest_flag = 1;
                break;
            case 's':
                DCOPY_SRC_PATH = realpath(optarg, NULL);
                src_flag = 1;
                break;
            case '?':
                print_usage(argv[0]);

                if (optopt == 'd' || optopt == 's')
                    fprintf(stderr, "Option -%c requires an argument.\n", optopt);
                else if (isprint (optopt))
                    fprintf(stderr, "Unknown option `-%c'.\n", optopt);
                else
                    fprintf(stderr,
                        "Unknown option character `\\x%x'.\n",
                        optopt);

                exit(EXIT_FAILURE);
            default:
                abort();
        }
    }

    for (index = optind; index < argc; index++)
    {
        print_usage(argv[0]);
        printf ("Non-option argument %s\n", argv[index]);

        exit(EXIT_FAILURE);
    }

    if(src_flag == 0)
    {
        print_usage(argv[0]);
        fprintf(stdout, "Error: You must specify a source.\n");

        exit(EXIT_FAILURE);
    }

    if(dest_flag ==0)
    {
        print_usage(argv[0]);
        fprintf(stdout, "Error: you must specify a destination.\n");

        exit(EXIT_FAILURE);
    }

    DCOPY_global_rank = CIRCLE_init(argc, argv);

    CIRCLE_cb_create (&DCOPY_start);
    CIRCLE_cb_process(&DCOPY_copy);

    CIRCLE_begin();
    CIRCLE_finalize();

    exit(EXIT_SUCCESS);
}

/* EOF */
