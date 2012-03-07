#ifndef __DCP_H_
#define __DCP_H_

#define DCOPY_CHUNK_SIZE 4194304

typedef enum {
    COPY, CHECKSUM, STAT
} DCOPY_operation_code_t;

typedef struct {
    DCOPY_operation_code_t code;
    int chunk;
    char* operand;
} DCOPY_operation_t;

typedef struct {
    time_t time_started;
    time_t time_finished;
    size_t total_bytes_copied;
} DCOPY_statistics_t;

typedef struct {
    char* dest_path;
    char** src_path;
} DCOPY_options_t;

#include <libcircle.h>

#include "copy.h"
#include "filestat.h"
#include "checksum.h"

char* DCOPY_encode_operation(DCOPY_operation_code_t op, int chunk, char* operand);
DCOPY_operation_t* DCOPY_decode_operation(char* op);

void DCOPY_process_dir(char* dir, CIRCLE_handle* handle);
void DCOPY_add_objects(CIRCLE_handle* handle);
void DCOPY_process_objects(CIRCLE_handle* handle);

#endif /* __DCP_H_ */
