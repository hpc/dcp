/* See the file "COPYING" for the full license governing this code. */

#ifndef __COMMON_H_
#define __COMMON_H_

/* Autotool defines. */
#include "../config.h"

/* Common logging. */
#include "log.h"

/* Make sure we're using 64 bit file handling. */
#ifdef _FILE_OFFSET_BITS
#undef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif

#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE 1
#endif

#ifndef __USE_LARGEFILE64
#define __USE_LARGEFILE64
#endif

#ifndef _LARGEFILE_SOURCE
#define _LARGEFILE_SOURCE
#endif

/* Enable posix extensions (popen). */
#ifndef _BSD_SOURCE
#define _BSD_SOURCE 1
#endif

#include <libcircle.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>

/*
 * This is the size of each chunk to be processed (in bytes). In 2012, most
 * of our filesystems are using 4MB block sizes (4194304 bytes).
 */
#define DCOPY_CHUNK_SIZE (4194304)

#ifndef PATH_MAX
#define PATH_MAX (4096)
#endif

#ifndef _POSIX_ARG_MAX
#define MAX_ARGS 4096
#else
#define MAX_ARGS _POSIX_ARG_MAX
#endif

typedef enum {
    TREEWALK, COPY, CLEANUP, COMPARE
} DCOPY_operation_code_t;

typedef struct {
    /*
     * The max file size is 8EB, limited by the signed upcast of off64_t
     * to uint64_t.
     */
    uint64_t file_size;

    /*
     * Assuming a 4MB chunk size, the maximum size of a chunked file is 16PB.
     * This is limited by counting chunks in uint32_t (2^32 * 4MB).
     */
    uint32_t chunk;

    /*
     * This offset represents the index into the operand path that gives the
     * starting index of the root path to copy from.
     */
    uint16_t source_base_offset;

    /* The operation type. */
    DCOPY_operation_code_t code;

    /* The full source path. */
    char* operand;

    /*
     * If the destination already existed before this copy started, we want to
     * copy the files to a location inside the destination. This is to keep
     * track of the path inside the destination (especially in the recursive
     * case).
     */
    char* dest_base_appendix;
} DCOPY_operation_t;

typedef struct {
    uint64_t total_bytes_copied;
    time_t   time_started;
    time_t   time_ended;
    double   wtime_started;
    double   wtime_ended;
} DCOPY_statistics_t;

typedef struct {
    char*  dest_path;
    char** src_path;
    bool   conditional;
    bool   skip_compare;
    bool   force;
    bool   preserve;
    bool   recursive;
    bool   recursive_unspecified;
    bool   reliable_filesystem;
} DCOPY_options_t;

DCOPY_operation_t* DCOPY_decode_operation(char* op);

char* DCOPY_encode_operation(DCOPY_operation_code_t code, \
                             uint32_t chunk, \
                             char* operand, \
                             uint16_t source_base_offset, \
                             char* dest_base_appendix, \
                             uint64_t file_size);

void DCOPY_retry_failed_operation(DCOPY_operation_code_t target, \
                                  CIRCLE_handle* handle, \
                                  DCOPY_operation_t* op);

void DCOPY_add_objects(CIRCLE_handle* handle);

void DCOPY_process_objects(CIRCLE_handle* handle);

void DCOPY_unlink_destination(DCOPY_operation_t* op);

FILE* DCOPY_open_input_stream(DCOPY_operation_t* op);

FILE* DCOPY_open_output_stream(DCOPY_operation_t* op);

int DCOPY_open_output_fd(DCOPY_operation_t* op);

#endif /* __COMMON_H_ */
