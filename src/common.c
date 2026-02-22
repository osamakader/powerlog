#include "common.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

int read_sysfs(const char *path, char *buf, size_t buf_size)
{
	FILE *f;
	size_t n;

	if (!path || !buf || buf_size == 0)
		return -1;

	f = fopen(path, "r");
	if (!f)
		return -1;

	n = fread(buf, 1, buf_size - 1, f);
	buf[n] = '\0';
	fclose(f);

	/* Trim trailing newline */
	if (n > 0 && buf[n - 1] == '\n')
		buf[--n] = '\0';

	return (int)n;
}

bool path_exists(const char *path)
{
	return access(path, F_OK) == 0;
}

void json_escape_fprintf(FILE *out, const char *s)
{
	if (!s)
		return;
	for (; *s; s++) {
		switch (*s) {
		case '"':  fputs("\\\"", out); break;
		case '\\': fputs("\\\\", out); break;
		case '\b': fputs("\\b", out); break;
		case '\f': fputs("\\f", out); break;
		case '\n': fputs("\\n", out); break;
		case '\r': fputs("\\r", out); break;
		case '\t': fputs("\\t", out); break;
		default:
			if ((unsigned char)*s < 0x20)
				fprintf(out, "\\u%04x", (unsigned char)*s);
			else
				fputc(*s, out);
			break;
		}
	}
}
