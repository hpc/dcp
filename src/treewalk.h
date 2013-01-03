/* See the file "COPYING" for the full license governing this code. */

#ifndef __DCP_TREEWALK_H
#define __DCP_TREEWALK_H

bool DCOPY_is_directory(char* path);
bool DCOPY_is_regular_file(char* path);
void DCOPY_do_treewalk(DCOPY_operation_t* op, CIRCLE_handle* handle);
void DCOPY_stat_process_file(DCOPY_operation_t* op, uint64_t file_size, CIRCLE_handle* handle);
void DCOPY_stat_process_dir(DCOPY_operation_t* op, CIRCLE_handle* handle);

#endif /* __DCP_TREEWALK_H */
