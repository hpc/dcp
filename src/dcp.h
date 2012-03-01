#ifndef __DCP_H_
#define __DCP_H_

#include <libcircle.h>

#define CHUNK_SIZE 4194304

typedef enum {
    COPY, CHECKSUM, STAT
} DCOPY_operation_code_t;

typedef struct {
    DCOPY_operation_code_t code;
    int chunk;
    char* operand;
} DCOPY_operation_t;

char* DCOPY_encode_operation(DCOPY_operation_code_t op, int chunk, char* operand);
DCOPY_operation_t* DCOPY_decode_operation(char* op);

void DCOPY_do_checksum(DCOPY_operation_t* op, CIRCLE_handle* handle);
void DCOPY_process_dir(char* dir, CIRCLE_handle* handle);
void DCOPY_do_stat(DCOPY_operation_t* op, CIRCLE_handle* handle);
void DCOPY_do_copy(DCOPY_operation_t* op, CIRCLE_handle* handle);
void DCOPY_add_objects(CIRCLE_handle* handle);
void DCOPY_process_objects(CIRCLE_handle* handle);

#endif /* __DCP_H_ */
