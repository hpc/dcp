/* See the file "COPYING" for the full license governing this code. */

#ifndef __DCP_FILESTAT_H
#define __DCP_FILESTAT_H

bool DCOPY_is_directory(char* path);
bool DCOPY_is_regular_file(char* path);
void DCOPY_do_stat(DCOPY_operation_t* op, CIRCLE_handle* handle);
void DCOPY_process_dir(char* dir, CIRCLE_handle* handle, uint16_t base_index);

#endif /* __DCP_FILESTAT_H */
