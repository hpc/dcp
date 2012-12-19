/* See the file "COPYING" for the full license governing this code. */

#ifndef __DCP_H_
#define __DCP_H_

#include <libcircle.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define DCOPY_CHUNK_SIZE 4194304

#ifndef PATH_MAX
  #define PATH_MAX (4096)
#endif

typedef enum {
    COPY, COMPARE, STAT
} DCOPY_operation_code_t;

typedef struct {
    DCOPY_operation_code_t code;
    uint32_t chunk;
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
    uint16_t dest_base_index;
    bool skip_compare;
    bool force;
    bool preserve;
    bool recursive;
    bool recursive_unspecified;
    struct stat dest_stat;
} DCOPY_options_t;

DCOPY_operation_t* DCOPY_decode_operation(char* op);
char* DCOPY_encode_operation(DCOPY_operation_code_t op, uint32_t chunk, char *operand);

void DCOPY_add_objects(CIRCLE_handle* handle);
void DCOPY_process_objects(CIRCLE_handle* handle);

void DCOPY_epilogue(void);
void DCOPY_print_version(char** argv);
void DCOPY_print_usage(char** argv);

#endif /* __DCP_H_ */
