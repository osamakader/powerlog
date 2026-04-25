#ifndef COMMON_H
#define COMMON_H

#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

#define SYSFS_PATH_MAX 256
#define READ_BUF_SIZE  64
#define LOG_TEXT_WIDTH 72

/* Generic text heading with a blank line, body line, and underline fill. */
void log_text_heading(FILE *out, char rule, const char *fmt, ...);

/* Human-readable text log: banner and section underlines */
void log_text_timestamp(FILE *out, const char *timestamp);
void log_text_sample_index(FILE *out, unsigned sample_no);
void log_text_section(FILE *out, const char *title);

/* Read contents of a sysfs file into buf, null-terminated. Returns bytes read or -1 on error. */
int read_sysfs(const char *path, char *buf, size_t buf_size);

/* Check if path exists */
bool path_exists(const char *path);

/* Write a complete JSON string literal with RFC 8259 escaping. */
void json_fprintf_string(FILE *out, const char *s);

#endif /* COMMON_H */
