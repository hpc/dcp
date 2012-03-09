/* See the file "COPYING" for the full license governing this code. */

#ifndef __DCP_H_
#define __DCP_H_

#include <time.h>
#include <libcircle.h>

#define DCOPY_CHUNK_SIZE 4194304

typedef enum {
    COPY, CHECKSUM, STAT
} DCOPY_operation_code_t;

typedef struct {
    DCOPY_operation_code_t code;
    uint32_t chunk;
    uint16_t base_index;
    char* operand;
} DCOPY_operation_t;

typedef struct {
    time_t time_started;
    time_t time_ended;
    double wtime_started;
    double wtime_ended;
    size_t total_bytes_copied;
} DCOPY_statistics_t;

typedef struct {
    char* dest_path;
    char** src_path;
} DCOPY_options_t;

DCOPY_operation_t* DCOPY_decode_operation(char* op);
char* DCOPY_encode_operation(DCOPY_operation_code_t op, uint32_t chunk, \
    char* operand, uint16_t base_index);

void DCOPY_add_objects(CIRCLE_handle* handle);
void DCOPY_process_objects(CIRCLE_handle* handle);

void DCOPY_init_jump_table(void);
void DCOPY_epilogue(void);
void DCOPY_print_version(char** argv);
void DCOPY_print_usage(char** argv);

#endif /* __DCP_H_ */
