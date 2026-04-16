#ifndef COMMON_H
#define COMMON_H

#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

#define SYSFS_PATH_MAX 256
#define READ_BUF_SIZE  64
#define LOG_TEXT_WIDTH 72

/* Human-readable text log: banner and section underlines */
void log_text_timestamp(FILE *out, const char *timestamp);
void log_text_section(FILE *out, const char *title);

/* Read contents of a sysfs file into buf, null-terminated. Returns bytes read or -1 on error. */
int read_sysfs(const char *path, char *buf, size_t buf_size);

/* Check if path exists */
bool path_exists(const char *path);

/* Write JSON string content (RFC 8259 escapes); UTF-8 multibyte sequences pass through */
void json_escape_fprintf(FILE *out, const char *s);

#endif /* COMMON_H */
