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
DCOPY_add_objects(CIRCLE_handle *handle)
{
    handle->enqueue(DCOPY_SRC_PATH);
}

void
DCOPY_process_objects(CIRCLE_handle *handle)
{
    DIR *current_dir;
    char temp[CIRCLE_MAX_STRING_LEN];
    char stat_temp[CIRCLE_MAX_STRING_LEN];
    struct dirent *current_ent;
    struct stat st;

    /* Pop an item off the queue */
    handle->dequeue(temp);
    LOG(DCOPY_LOG_DBG, "Popped [%s]", temp);

    /* Try and stat it, checking to see if it is a link */
    if(lstat(temp,&st) != EXIT_SUCCESS)
    {
        LOG(DCOPY_LOG_ERR, "Error: Couldn't stat \"%s\"", temp);
        //MPI_Abort(MPI_COMM_WORLD,-1);
    }
    /* Check to see if it is a directory.  If so, put its children in the queue */
    else if(S_ISDIR(st.st_mode) && !(S_ISLNK(st.st_mode)))
    {
        current_dir = opendir(temp);

        if(!current_dir)
        {
            LOG(DCOPY_LOG_ERR, "Unable to open dir");
        }
        else
        {
            /* Since it's quick, just create the directory at the destination. */
            char * new_dir_name = malloc(snprintf(NULL, 0, "%s/%s", DCOPY_DEST_PATH, temp) + 1);
            sprintf(new_dir_name, "%s/%s", DCOPY_DEST_PATH, temp);
            LOG(DCOPY_LOG_DBG, "Creating directory with name: %s", new_dir_name);
            mkdir(new_dir_name, st.st_mode);
            free(new_dir_name);

            /* Read in each directory entry */
            while((current_ent = readdir(current_dir)) != NULL)
            {
            /* We don't care about . or .. */
            if((strncmp(current_ent->d_name,".",2)) && (strncmp(current_ent->d_name,"..",3)))
                {
                    strcpy(stat_temp,temp);
                    strcat(stat_temp,"/");
                    strcat(stat_temp,current_ent->d_name);

                    LOG(DCOPY_LOG_DBG, "Pushing [%s] <- [%s]", stat_temp, temp);
                    handle->enqueue(&stat_temp[0]);
                }
            }
        }
        closedir(current_dir);
    }
    //else if(S_ISREG(st.st_mode) && (st.st_size % 4096 == 0))
    else if(S_ISREG(st.st_mode)) {
        LOG(DCOPY_LOG_DBG, "Copying: %s", temp);

        char *base_name = basename(temp);
        char *new_file_name = malloc(snprintf(NULL, 0, "%s/%s", DCOPY_DEST_PATH, base_name) + 1);
        sprintf(new_file_name, "%s/%s", DCOPY_DEST_PATH, base_name);

        FILE *fp;
        char cmd[CIRCLE_MAX_STRING_LEN + 10];
        char out[1035];

        sprintf(cmd, "cp %s %s", temp, new_file_name);

        fp = popen(cmd, "r");
        if (fp == NULL) {
            printf("Failed to run command\n" );
        }

        while (fgets(out, sizeof(out) - 1, fp) != NULL) {
            printf("%s", out);
        }

        pclose(fp);
    }
}

int
DCOPY_copy_data(FILE *fin, FILE *fout)
{
    char   *buffer = (char *)malloc(DCOPY_FILECOPY_BUFFER_SIZE);
    size_t n;

    LOG(DCOPY_LOG_DBG, "Copying the file.");

    while ((n = fread(buffer, sizeof(char), sizeof(buffer), fin)) > 0)
    {
        if (fwrite(buffer, sizeof(char), n, fout) != n)
        {
            LOG(DCOPY_LOG_FATAL, "Writing a file failed.");
            return -1;
        }
    }

    return 0;
}

FILE *
DCOPY_open_infile(char *infile)
{
    FILE *fin = fopen(infile, "rb");

    if (fin == NULL) {
        LOG(DCOPY_LOG_ERR, "Could not open the source file: %s", infile);
        return NULL;
    }

    return fin;
}

FILE *
DCOPY_open_outfile(char *outfile, FILE *fin)
{
    FILE *fout;

    if(outfile == NULL)
    {
        LOG(DCOPY_LOG_ERR, "Attempted to open a bad filename.");
        fclose(fin);
        return NULL;
    }
    else
    {
        fout = fopen(outfile, "wb");
    }

    if (fout == NULL)
    {
        LOG(DCOPY_LOG_ERR, "Could not open the destination file: %s", outfile);
        fclose(fin);
        return NULL;
    }

    return fout;
}

int
main (int argc, char **argv)
{
    int index;
    int c;

    DCOPY_debug_stream = stdout;
    DCOPY_debug_level  = DCOPY_LOG_DBG;
     
    opterr = 0;
    while((c = getopt(argc, argv, "s:d:")) != -1)
    {
        switch(c)
        {
            case 'd':
                DCOPY_DEST_PATH = realpath(optarg, NULL);
                break;
            case 's':
                DCOPY_SRC_PATH = realpath(optarg, NULL);
                break;
            case '?':
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
        printf ("Non-option argument %s\n", argv[index]);

    DCOPY_global_rank = CIRCLE_init(argc, argv);

    CIRCLE_cb_create (&DCOPY_add_objects);
    CIRCLE_cb_process(&DCOPY_process_objects);

    CIRCLE_begin();
    CIRCLE_finalize();

    free(DCOPY_DEST_PATH);
    free(DCOPY_SRC_PATH);

    exit(EXIT_SUCCESS);
}

/* EOF */
