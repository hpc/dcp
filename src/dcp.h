#ifndef WARNUSERS_H
#define WARNUSERS_H

#include <libcircle.h>

void add_objects(CIRCLE_handle *handle);
void process_objects(CIRCLE_handle *handle);
int warnusers_create_redis_attr_cmd(char *buf, struct stat *st, char *filename, char *filekey);
int warnusers_redis_run_zadd(char *filekey, long val, char *zset, char *filename);
int warnusers_redis_run_scard(char * set);
int warnusers_redis_run_get(char * key);
int warnusers_redis_run_get_str(char * key, char * str);
int warnusers_redis_run_cmd(char *cmd, char *filename);
int warnusers_redis_keygen(char *buf, char *filename);
void warnusers_get_uids(CIRCLE_handle *handle);
void print_usage(char **argv);
void warnusers_redis_run_sadd(int uid);
int warnusers_redis_run_spop(char * uid);
#endif /* WARNUSERS_H */
