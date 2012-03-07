/** The loglevel that this instance of dcopy will output. */
extern DCOPY_loglevel  DCOPY_debug_level;

void DCOPY_do_stat(DCOPY_operation_t* op, CIRCLE_handle* handle)
{
    static struct stat st;
    static int status;
    int is_top_dir = !strcmp(op->operand, TOP_DIR);
    char path[4096];

    if(is_top_dir) {
        sprintf(path, "%s", TOP_DIR);
    }
    else {
        sprintf(path, "%s/%s", TOP_DIR, op->operand);
    }

    status = lstat(path, &st);

    if(status != EXIT_SUCCESS) {
        LOG(DCOPY_LOG_ERR, "Unable to stat \"%s\"", path);
        perror("stat");
    }
    else if(S_ISDIR(st.st_mode) && !(S_ISLNK(st.st_mode))) {
        char dir[2048];
        LOG(DCOPY_LOG_DBG, "Operand: %s Dir: %s", op->operand, DEST_DIR);

        if(is_top_dir) {
            sprintf(dir, "mkdir -p %s", op->operand);
        }
        else {
            sprintf(dir, "mkdir -p %s/%s", DEST_DIR, op->operand);
        }

        LOG(DCOPY_LOG_DBG, "Creating %s", dir);

        FILE* p = popen(dir, "r");
        pclose(p);
        DCOPY_process_dir(op->operand, handle);
    }
    else {
        int num_chunks = st.st_size / CHUNK_SIZE;
        LOG(DCOPY_LOG_DBG, "File size: %ld Chunks:%d Total: %d", st.st_size, num_chunks, num_chunks * CHUNK_SIZE);
        int i = 0;

        for(i = 0; i < num_chunks; i++) {
            char* newop = DCOPY_encode_operation(COPY, i, op->operand);
            handle->enqueue(newop);
            free(newop);
        }

        if(num_chunks * CHUNK_SIZE < st.st_size) {
            char* newop = DCOPY_encode_operation(COPY, i, op->operand);
            handle->enqueue(newop);
            free(newop);
        }
    }

    return;
}

/* EOF */
