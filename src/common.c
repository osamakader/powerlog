#include "common.h"
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>

int read_sysfs(const char *path, char *buf, size_t buf_size)
{
	FILE *f;
	size_t n;

	if (!path || !buf || buf_size == 0)
		return -1;

	buf[0] = '\0';

	f = fopen(path, "r");
	if (!f)
		return -1;

	n = fread(buf, 1, buf_size - 1, f);
	if (ferror(f)) {
		fclose(f);
		buf[0] = '\0';
		return -1;
	}
	buf[n] = '\0';
	fclose(f);

	/* Trim trailing newline (sysfs values are usually one line) */
	if (n > 0 && buf[n - 1] == '\n')
		buf[--n] = '\0';

	return (int)n;
}

bool path_exists(const char *path)
{
	return access(path, F_OK) == 0;
}

static void log_text_rule(FILE *out, char c)
{
	int i;

	for (i = 0; i < LOG_TEXT_WIDTH; i++)
		fputc((int)c, out);
	fputc('\n', out);
}

void log_text_heading(FILE *out, char rule, const char *fmt, ...)
{
	va_list ap;

	fputc('\n', out);
	log_text_rule(out, rule);
	va_start(ap, fmt);
	vfprintf(out, fmt, ap);
	va_end(ap);
	fputc('\n', out);
	log_text_rule(out, rule);
}

void log_text_timestamp(FILE *out, const char *timestamp)
{
	log_text_heading(out, '=', "  %s", timestamp);
}

void log_text_sample_index(FILE *out, unsigned sample_no)
{
	log_text_heading(out, '=', "  Sample %u", sample_no);
}

void log_text_section(FILE *out, const char *title)
{
	log_text_heading(out, '-', "%s", title);
}

static void json_fprintf_escaped(FILE *out, const char *s)
{
	unsigned char c;

	if (!s)
		return;

	while ((c = (unsigned char)*s++) != '\0') {
		switch (c) {
		case '"':
			fputc('\\', out);
			fputc('"', out);
			break;
		case '\\':
			fputc('\\', out);
			fputc('\\', out);
			break;
		case '\b':
			fputs("\\b", out);
			break;
		case '\f':
			fputs("\\f", out);
			break;
		case '\n':
			fputs("\\n", out);
			break;
		case '\r':
			fputs("\\r", out);
			break;
		case '\t':
			fputs("\\t", out);
			break;
		default:
			/* RFC 8259: control chars U+0000–U+001F as \uXXXX; UTF-8 bytes pass through */
			if (c < 0x20u)
				fprintf(out, "\\u%04x", c);
			else
				fputc((int)c, out);
			break;
		}
	}
}

void json_fprintf_string(FILE *out, const char *s)
{
	fputc('"', out);
	json_fprintf_escaped(out, s ? s : "");
	fputc('"', out);
}
