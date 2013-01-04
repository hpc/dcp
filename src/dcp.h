/* See the file "COPYING" for the full license governing this code. */

#ifndef __DCP_H_
#define __DCP_H_

#include "common.h"

DCOPY_operation_t* DCOPY_decode_operation(char* op);
char* DCOPY_encode_operation(DCOPY_operation_code_t op, uint32_t chunk, \
                             char *operand, uint16_t source_base_offset, \
                             char *dest_base_appendix, uint64_t file_size);

void DCOPY_add_objects(CIRCLE_handle* handle);
void DCOPY_process_objects(CIRCLE_handle* handle);

void DCOPY_epilogue(void);
void DCOPY_print_version(void);
void DCOPY_print_usage(char** argv);

#endif /* __DCP_H_ */
