#ifndef LOG_H
#define LOG_H

#include <stdio.h>

typedef enum
{
    DCOPY_LOG_FATAL = 1,
    DCOPY_LOG_ERR   = 2,
    DCOPY_LOG_WARN  = 3,
    DCOPY_LOG_INFO  = 4,
    DCOPY_LOG_DBG   = 5
} DCOPY_loglevel;

#define LOG(level, ...) do {  \
        if (level <= DCOPY_debug_level) { \
            fprintf(DCOPY_debug_stream,"%d:%s:%d:", DCOPY_global_rank, __FILE__, __LINE__); \
            fprintf(DCOPY_debug_stream, __VA_ARGS__); \
            fprintf(DCOPY_debug_stream, "\n"); \
            fflush(DCOPY_debug_stream); \
        } \
    } while (0)

extern int DCOPY_global_rank;
extern FILE *DCOPY_debug_stream;
extern DCOPY_loglevel DCOPY_debug_level;

#endif /* LOG_H */
