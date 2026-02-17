#ifndef COMMON_H
#define COMMON_H

#include <stddef.h>
#include <stdbool.h>

#define SYSFS_PATH_MAX 256
#define READ_BUF_SIZE  64

/* Read contents of a sysfs file into buf, null-terminated. Returns bytes read or -1 on error. */
int read_sysfs(const char *path, char *buf, size_t buf_size);

/* Check if path exists */
bool path_exists(const char *path);

#endif /* COMMON_H */
