#ifndef DCOPY_H
#define DCOPY_H

#include <unistd.h>
#include <stdint.h>

#ifndef DCOPY_FILECOPY_BUFFER_SIZE
    #define DCOPY_FILECOPY_BUFFER_SIZE (32 * 1024 * 1024) > (SIZE_MAX / 2) ? \
        (SIZE_MAX / 2) : (32 * 1024 * 1024)
#endif

void DCOPY_add_objects    ( CIRCLE_handle *handle );
void DCOPY_process_objects( CIRCLE_handle *handle );

int    DCOPY_copy_data   ( FILE *fin, FILE *fout );
FILE * DCOPY_open_infile ( char *infile );
FILE * DCOPY_open_outfile( char *outfile, FILE *fdin );

#endif /* DCOPY_H */
