/* See the file "COPYING" for the full license governing this code. */

#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <time.h>

typedef enum {
    DCOPY_LOG_FATAL = 1,
    DCOPY_LOG_ERR   = 2,
    DCOPY_LOG_WARN  = 3,
    DCOPY_LOG_INFO  = 4,
    DCOPY_LOG_DBG   = 5
} DCOPY_loglevel;

#define LOG(level, ...) do {  \
        if (level <= DCOPY_debug_level) { \
            char timestamp[256]; \
            time_t ltime = time(NULL); \
            struct tm *ttime = localtime(&ltime); \
            strftime(timestamp, sizeof(timestamp), \
                     "%Y-%m-%dT%H:%M:%S", ttime); \
            if(level == DCOPY_LOG_DBG) { \
                fprintf(DCOPY_debug_stream,"[%s] [%d] [%s:%d] ", \
                        timestamp, CIRCLE_global_rank, \
                        __FILE__, __LINE__); \
            } else { \
                fprintf(DCOPY_debug_stream,"[%s] ", timestamp); \
            } \
            fprintf(DCOPY_debug_stream, __VA_ARGS__); \
            fprintf(DCOPY_debug_stream, "\n"); \
            fflush(DCOPY_debug_stream); \
        } \
    } while (0)

extern int CIRCLE_global_rank;
extern FILE* DCOPY_debug_stream;
extern DCOPY_loglevel DCOPY_debug_level;

#endif /* LOG_H */
