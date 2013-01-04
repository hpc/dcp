/* See the file "COPYING" for the full license governing this code. */

#ifndef __COMMON_H_
#define __COMMON_H_

/* Autotool defines. */
#include "../config.h"

/* Common logging. */
#include "log.h"

/* Make sure we're using 64 bit file handling. */
#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE 1
#endif

/* Enable posix extensions (popen). */
#ifndef _BSD_SOURCE
#define _BSD_SOURCE 1
#endif

#include <libcircle.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
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
    uint64_t file_size; /* Max 16EB total file size (Limited by chunk count) */
    uint32_t chunk;     /* Max 16PB chunked file (Assuming 4MB chunk size) */
    uint16_t source_base_offset;
    DCOPY_operation_code_t code;
    char* operand;
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
    struct stat dest_stat;
} DCOPY_options_t;

#endif /* __COMMON_H_ */
